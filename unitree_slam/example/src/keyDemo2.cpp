#include <unitree/robot/client/client.hpp>
#include <unitree/robot/channel/channel_subscriber.hpp>
#include <unitree/robot/go2/sport/sport_client.hpp>
#include <unitree/idl/ros2/String_.hpp>
#include <json.hpp>
#include <termio.h>
#include <string>
#include <future>
#include <thread>
#include <atomic>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>

#define SlamInfoTopic "rt/slam_info"
#define SlamKeyInfoTopic "rt/slam_key_info"
using namespace unitree::robot;
using namespace unitree::common;
unsigned char currentKey;

class poseDate
{
public:
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float q_x = 0.0f;
    float q_y = 0.0f;
    float q_z = 0.0f;
    float q_w = 1.0f;
    int mode = 1;
    float speed = 0.8f;
    std::string toJsonStr() const
    {
        nlohmann::json j;
        j["data"]["targetPose"]["x"] = x;
        j["data"]["targetPose"]["y"] = y;
        j["data"]["targetPose"]["z"] = z;
        j["data"]["targetPose"]["q_x"] = q_x;
        j["data"]["targetPose"]["q_y"] = q_y;
        j["data"]["targetPose"]["q_z"] = q_z;
        j["data"]["targetPose"]["q_w"] = q_w;
        j["data"]["mode"] = mode;
        j["data"]["speed"] = speed;
        return j.dump(4);
    }
    void printInfo() const
    {
        std::cout << "x:" << x << " y:" << y << " z:" << z << " q_x:"
                  << q_x << " q_y:" << q_y << " q_z:" << q_z << " q_w:" << q_w << std::endl;
    }
};

namespace unitree::robot::slam
{

    const std::string TEST_SERVICE_NAME = "slam_operate";
    const std::string TEST_API_VERSION = "1.0.0.1";

    const int32_t ROBOT_API_ID_STOP_NODE = 1901;
    const int32_t ROBOT_API_ID_START_MAPPING_PL = 1801;
    const int32_t ROBOT_API_ID_END_MAPPING_PL = 1802;
    const int32_t ROBOT_API_ID_START_RELOCATION_PL = 1804;
    const int32_t ROBOT_API_ID_POSE_NAV_PL = 1102;
    const int32_t ROBOT_API_ID_PAUSE_NAV = 1201;
    const int32_t ROBOT_API_ID_RESUME_NAV = 1202;

    class TestClient : public Client
    {
    private:
        ChannelSubscriberPtr<std_msgs::msg::dds_::String_> subSlamInfo;
        ChannelSubscriberPtr<std_msgs::msg::dds_::String_> subSlamKeyInfo;

        void slamInfoHandler(const void *message);
        void slamKeyInfoHandler(const void *message);

        poseDate curPose;

        // Phase 2: per-floor task lists
        std::vector<poseDate> poseList_f1;
        std::vector<poseDate> poseList_f2;

        bool is_arrived = false;
        bool threadControl = false;
        std::future<void> futThread;
        std::promise<void> prom;
        std::thread controlThread;

        // Phase 1: direct Go2 body control during blind stair climbing.
        unitree::robot::go2::SportClient sportClient;
        std::atomic<bool> is_climbing{false};
        std::thread climbThread;

    public:
        // Phase 1 tunables
        float climb_vx = 0.35f; // forward velocity while climbing (m/s), adjust 0.2~0.5

        // Phase 2: map file paths and persistence targets
        std::string floor1_pcd = "/home/unitree/floor1.pcd";
        std::string floor2_pcd = "/home/unitree/floor2.pcd";
        std::string taskListPath_f1 = "f1.json";
        std::string taskListPath_f2 = "f2.json";
        int currentFloor = 0; // 0=unloaded, 1=floor1, 2=floor2

        TestClient();
        ~TestClient();

        void Init();
        unsigned char keyDetection();
        unsigned char keyExecute();

        // Core slam ops
        void stopNodeFun();
        void startMappingPlFun();
        void endMappingPlFun(const std::string &pcd);
        void relocationPlFun(const std::string &pcd, const poseDate &init);
        void taskLoopFun(std::promise<void> &prom);
        void pauseNavFun();
        void resumeNavFun();
        void taskThreadRun();
        void taskThreadStop();

        // Phase 1: blind stair-climbing toggle
        void climbStairsFun();

        // Phase 2: helpers
        int chooseFloor(const char *prompt);
        void saveTaskListFun();
        void loadTaskListFun();
    };

    TestClient::TestClient() : Client(TEST_SERVICE_NAME, false)
    {
        subSlamInfo = ChannelSubscriberPtr<std_msgs::msg::dds_::String_>(new ChannelSubscriber<std_msgs::msg::dds_::String_>(SlamInfoTopic));
        subSlamInfo->InitChannel(std::bind(&unitree::robot::slam::TestClient::slamInfoHandler, this, std::placeholders::_1), 1);
        subSlamKeyInfo = ChannelSubscriberPtr<std_msgs::msg::dds_::String_>(new ChannelSubscriber<std_msgs::msg::dds_::String_>(SlamKeyInfoTopic));
        subSlamKeyInfo->InitChannel(std::bind(&unitree::robot::slam::TestClient::slamKeyInfoHandler, this, std::placeholders::_1), 1);
        std::cout << "********************  Unitree SLAM Demo (keyDemo2) ***************\n";
        std::cout << "---------------            q    w                -----------------\n";
        std::cout << "---------------            a    s   d   f        -----------------\n";
        std::cout << "---------------            z    x   c   S        -----------------\n";
        std::cout << "------------------------------------------------------------------\n";
        std::cout << "------------------ q: Start mapping             ------------------\n";
        std::cout << "------------------ w: End mapping (pick 1 or 2) ------------------\n";
        std::cout << "------------------ a: Relocation   (pick 1 or 2)------------------\n";
        std::cout << "------------------ s: Add pose to current floor ------------------\n";
        std::cout << "------------------ d: Execute current floor list------------------\n";
        std::cout << "------------------ f: Clear current floor list  ------------------\n";
        std::cout << "------------------ z: Pause navigation          ------------------\n";
        std::cout << "------------------ x: Resume navigation         ------------------\n";
        std::cout << "------------------ c: Climb stairs TOGGLE       ------------------\n";
        std::cout << "------------------ S: Save both lists to json   ------------------\n";
        std::cout << "---------------- Press any other key to stop SLAM ----------------\n";
        std::cout << "------------------------------------------------------------------\n";
        std::cout << "--------------- Press 'Ctrl + C' to exit the program -------------\n";
        std::cout << "------------------------------------------------------------------\n"
                  << std::endl;
    }

    TestClient::~TestClient()
    {
        if (is_climbing.load())
        {
            is_climbing.store(false);
            if (climbThread.joinable())
                climbThread.join();
            sportClient.StopMove();
        }
        stopNodeFun();
    }

    void TestClient::Init()
    {
        SetApiVersion(TEST_API_VERSION);

        UT_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_POSE_NAV_PL);
        UT_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_PAUSE_NAV);
        UT_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_RESUME_NAV);
        UT_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_STOP_NODE);
        UT_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_START_MAPPING_PL);
        UT_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_END_MAPPING_PL);
        UT_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_START_RELOCATION_PL);

        // Initialize the Go2 sport service for direct body control during blind stair climbing.
        sportClient.SetTimeout(10.0f);
        sportClient.Init();
    }
}

void unitree::robot::slam::TestClient::taskThreadRun()
{
    taskThreadStop();
    prom = std::promise<void>();
    futThread = prom.get_future();
    controlThread = std::thread(&unitree::robot::slam::TestClient::taskLoopFun, this, std::ref(prom));
    controlThread.detach();
}

void unitree::robot::slam::TestClient::taskLoopFun(std::promise<void> &prom)
{
    std::string data;
    threadControl = true;

    std::vector<poseDate> *listPtr = nullptr;
    if (currentFloor == 1)
        listPtr = &poseList_f1;
    else if (currentFloor == 2)
        listPtr = &poseList_f2;

    if (!listPtr || listPtr->empty())
    {
        std::cout << "[d] No task list loaded for currentFloor=" << currentFloor
                  << ". Press 'a' to load a map first, then 's' to add points."
                  << std::endl;
        prom.set_value();
        return;
    }

    std::vector<poseDate> &poseList = *listPtr;
    std::cout << "Floor " << currentFloor << " task list num: " << poseList.size() << std::endl;

    for (int i = 0; i < (int)poseList.size(); i++)
    {
        is_arrived = false;
        int32_t statusCode = Call(ROBOT_API_ID_POSE_NAV_PL, poseList[i].toJsonStr(), data);
        std::cout << "parameter:" << poseList[i].toJsonStr() << std::endl;
        std::cout << "statusCode:" << statusCode << std::endl;
        std::cout << "data:" << data << std::endl;

        while (!is_arrived)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            if (!threadControl)
                break;
        }

        if (!threadControl)
            break;
    }

    std::cout << "Floor " << currentFloor << " task list done." << std::endl;
    prom.set_value();
}

void unitree::robot::slam::TestClient::taskThreadStop()
{
    threadControl = false;
    if (futThread.valid())
    {
        auto status = futThread.wait_for(std::chrono::milliseconds(0));
        if (status != std::future_status::ready)
            futThread.wait();
    }
}

void unitree::robot::slam::TestClient::slamInfoHandler(const void *message)
{
    std_msgs::msg::dds_::String_ currentMsg = *(std_msgs::msg::dds_::String_ *)message;
    nlohmann::json jsonData = nlohmann::json ::parse(currentMsg.data());

    if (jsonData["errorCode"] != 0)
    {
        std::cout << "\033[33m" << jsonData["info"] << "\033[0m" << std::endl;
        return;
    }

    if (jsonData["type"] == "pos_info")
    {
        curPose.x = jsonData["data"]["currentPose"]["x"];
        curPose.y = jsonData["data"]["currentPose"]["y"];
        curPose.z = jsonData["data"]["currentPose"]["z"];
        curPose.q_x = jsonData["data"]["currentPose"]["q_x"];
        curPose.q_y = jsonData["data"]["currentPose"]["q_y"];
        curPose.q_z = jsonData["data"]["currentPose"]["q_z"];
        curPose.q_w = jsonData["data"]["currentPose"]["q_w"];
    }
}

void unitree::robot::slam::TestClient::slamKeyInfoHandler(const void *message)
{
    std_msgs::msg::dds_::String_ currentMsg = *(std_msgs::msg::dds_::String_ *)message;
    nlohmann::json jsonData = nlohmann::json ::parse(currentMsg.data());

    if (jsonData["errorCode"] != 0)
    {
        std::cout << "\033[33m" << jsonData["info"] << "\033[0m" << std::endl;
        return;
    }

    if (jsonData["type"] == "task_result")
    {
        is_arrived = jsonData["data"]["is_arrived"];
        if (is_arrived)
        {
            std::cout << "I arrived " << jsonData["data"]["targetNodeName"] << std::endl;
        }
        else
        {
            std::cout << "I not arrived " << jsonData["data"]["targetNodeName"] << "  Please help me!!  (T_T)   (T_T)   (T_T) " << std::endl;
        }
    }
}

void unitree::robot::slam::TestClient::stopNodeFun()
{
    std::string parameter, data;
    parameter = R"({"data": {}})";
    int32_t statusCode = Call(ROBOT_API_ID_STOP_NODE, parameter, data);
    std::cout << "stopNode statusCode:" << statusCode << std::endl;
    std::cout << "data:" << data << std::endl;
}

void unitree::robot::slam::TestClient::startMappingPlFun()
{
    std::string parameter, data;
    parameter = R"({"data": {"slam_type": "indoor"}})";
    int32_t statusCode = Call(ROBOT_API_ID_START_MAPPING_PL, parameter, data);
    std::cout << "startMapping statusCode:" << statusCode << std::endl;
    std::cout << "data:" << data << std::endl;
}

void unitree::robot::slam::TestClient::endMappingPlFun(const std::string &pcd)
{
    std::string parameter, data;
    nlohmann::json j;
    j["data"]["address"] = pcd;
    parameter = j.dump();
    int32_t statusCode = Call(ROBOT_API_ID_END_MAPPING_PL, parameter, data);
    std::cout << "endMapping -> " << pcd << std::endl;
    std::cout << "statusCode:" << statusCode << std::endl;
    std::cout << "data:" << data << std::endl;
}

void unitree::robot::slam::TestClient::relocationPlFun(const std::string &pcd, const poseDate &init)
{
    std::string parameter, data;
    nlohmann::json j;
    j["data"]["x"] = init.x;
    j["data"]["y"] = init.y;
    j["data"]["z"] = init.z;
    j["data"]["q_x"] = init.q_x;
    j["data"]["q_y"] = init.q_y;
    j["data"]["q_z"] = init.q_z;
    j["data"]["q_w"] = init.q_w;
    j["data"]["address"] = pcd;
    parameter = j.dump(4);
    int32_t statusCode = Call(ROBOT_API_ID_START_RELOCATION_PL, parameter, data);
    std::cout << "relocation <- " << pcd << ", init pose: ";
    init.printInfo();
    std::cout << "statusCode:" << statusCode << std::endl;
    std::cout << "data:" << data << std::endl;
}

void unitree::robot::slam::TestClient::pauseNavFun()
{
    std::string parameter, data;
    parameter = R"({"data": {}})";
    int32_t statusCode = Call(ROBOT_API_ID_PAUSE_NAV, parameter, data);
    std::cout << "pauseNav statusCode:" << statusCode << std::endl;
    std::cout << "data:" << data << std::endl;
}

void unitree::robot::slam::TestClient::resumeNavFun()
{
    std::string parameter, data;
    parameter = R"({"data": {}})";
    int32_t statusCode = Call(ROBOT_API_ID_RESUME_NAV, parameter, data);
    std::cout << "resumeNav statusCode:" << statusCode << std::endl;
    std::cout << "data:" << data << std::endl;
}

void unitree::robot::slam::TestClient::climbStairsFun()
{
    if (!is_climbing.load())
    {
        // Prevent the slam planner from fighting us while we blind-walk.
        is_climbing.store(true);
        pauseNavFun();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        sportClient.BalanceStand();

        climbThread = std::thread([this]() {
            // Keep publishing the forward velocity at ~50 Hz; Go2 auto-brakes
            // if commands stop arriving, so we must refresh continuously.
            while (is_climbing.load())
            {
                sportClient.Move(climb_vx, 0.0f, 0.0f);
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        });

        std::cout << "\033[1;36m"
                  << ">>> Climbing started (vx=" << climb_vx
                  << "). Press 'c' again to stop."
                  << "\033[0m" << std::endl;
    }
    else
    {
        is_climbing.store(false);
        if (climbThread.joinable())
            climbThread.join();
        sportClient.StopMove();
        sportClient.BalanceStand();
        resumeNavFun();

        std::cout << "\033[1;36m"
                  << ">>> Climbing ended. Dog is in BalanceStand, SLAM resumed."
                  << "\033[0m" << std::endl;
    }
}

int unitree::robot::slam::TestClient::chooseFloor(const char *prompt)
{
    // keyDetection() puts the terminal into non-canonical/no-echo mode only
    // for the duration of one getchar(). When we arrive here the terminal is
    // already in canonical mode, but we flip it explicitly to be safe in
    // case something earlier changed the flags.
    termios oldT;
    tcgetattr(0, &oldT);
    termios newT = oldT;
    newT.c_lflag |= (ICANON | ECHO);
    tcsetattr(0, TCSANOW, &newT);

    std::cout << prompt << " [1/2]: " << std::flush;
    int idx = 0;
    if (!(std::cin >> idx))
    {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        idx = 0;
    }
    else
    {
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    tcsetattr(0, TCSANOW, &oldT);
    return idx;
}

void unitree::robot::slam::TestClient::saveTaskListFun()
{
    auto dumpList = [](const std::vector<poseDate> &list, const std::string &path) -> bool {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto &p : list)
        {
            nlohmann::json j;
            j["x"] = p.x;
            j["y"] = p.y;
            j["z"] = p.z;
            j["q_x"] = p.q_x;
            j["q_y"] = p.q_y;
            j["q_z"] = p.q_z;
            j["q_w"] = p.q_w;
            j["mode"] = p.mode;
            j["speed"] = p.speed;
            arr.push_back(j);
        }
        std::ofstream f(path);
        if (!f.is_open())
            return false;
        f << arr.dump(2);
        return true;
    };

    bool ok1 = dumpList(poseList_f1, taskListPath_f1);
    bool ok2 = dumpList(poseList_f2, taskListPath_f2);
    std::cout << "Saved floor1 (" << poseList_f1.size() << " pts) to "
              << taskListPath_f1 << (ok1 ? " [OK]" : " [FAIL]") << std::endl;
    std::cout << "Saved floor2 (" << poseList_f2.size() << " pts) to "
              << taskListPath_f2 << (ok2 ? " [OK]" : " [FAIL]") << std::endl;
}

void unitree::robot::slam::TestClient::loadTaskListFun()
{
    auto loadList = [](std::vector<poseDate> &list, const std::string &path) -> int {
        std::ifstream f(path);
        if (!f.is_open())
            return -1;
        nlohmann::json arr;
        try
        {
            f >> arr;
        }
        catch (...)
        {
            return -2;
        }
        if (!arr.is_array())
            return -2;
        list.clear();
        for (const auto &j : arr)
        {
            poseDate p;
            p.x = j.value("x", 0.0f);
            p.y = j.value("y", 0.0f);
            p.z = j.value("z", 0.0f);
            p.q_x = j.value("q_x", 0.0f);
            p.q_y = j.value("q_y", 0.0f);
            p.q_z = j.value("q_z", 0.0f);
            p.q_w = j.value("q_w", 1.0f);
            p.mode = j.value("mode", 1);
            p.speed = j.value("speed", 0.8f);
            list.push_back(p);
        }
        return (int)list.size();
    };

    int n1 = loadList(poseList_f1, taskListPath_f1);
    int n2 = loadList(poseList_f2, taskListPath_f2);
    if (n1 >= 0)
        std::cout << "Loaded floor1 " << n1 << " pts from " << taskListPath_f1 << std::endl;
    else
        std::cout << "No floor1 task list found at " << taskListPath_f1 << std::endl;
    if (n2 >= 0)
        std::cout << "Loaded floor2 " << n2 << " pts from " << taskListPath_f2 << std::endl;
    else
        std::cout << "No floor2 task list found at " << taskListPath_f2 << std::endl;
}

unsigned char unitree::robot::slam::TestClient::keyDetection()
{
    termios tms_old, tms_new;
    tcgetattr(0, &tms_old);
    tms_new = tms_old;
    tms_new.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(0, TCSANOW, &tms_new);
    unsigned char ch = getchar();
    tcsetattr(0, TCSANOW, &tms_old);
    std::cout << "\033[1;32m"
              << "Key " << ch << " pressed."
              << "\033[0m" << std::endl;
    return ch;
}

unsigned char unitree::robot::slam::TestClient::keyExecute()
{
    unsigned char currentKey;
    while (true)
    {
        currentKey = keyDetection();
        switch (currentKey)
        {
        case 'q':
            startMappingPlFun();
            break;

        case 'w':
        {
            int idx = chooseFloor("Save pcd as which floor");
            if (idx == 1)
                endMappingPlFun(floor1_pcd);
            else if (idx == 2)
                endMappingPlFun(floor2_pcd);
            else
                std::cout << "Invalid choice, expected 1 or 2" << std::endl;
            break;
        }

        case 'a':
        {
            int idx = chooseFloor("Load pcd of which floor");
            if (idx != 1 && idx != 2)
            {
                std::cout << "Invalid choice, expected 1 or 2" << std::endl;
                break;
            }
            if (currentFloor != 0)
            {
                // A slam node is already running (either mapping or a prior
                // relocation). We must stop it cleanly before starting a new
                // relocation on the other map.
                std::cout << "Stopping current slam node before switching map..." << std::endl;
                stopNodeFun();
                std::this_thread::sleep_for(std::chrono::milliseconds(1500));
            }
            const std::string &pcd = (idx == 1) ? floor1_pcd : floor2_pcd;
            relocationPlFun(pcd, poseDate{});
            currentFloor = idx;
            std::cout << ">>> Current floor is now " << currentFloor << std::endl;
            break;
        }

        case 's':
        {
            if (currentFloor == 0)
            {
                std::cout << "No map loaded; press 'a' first to pick a floor." << std::endl;
                break;
            }
            auto &list = (currentFloor == 1) ? poseList_f1 : poseList_f2;
            list.push_back(curPose);
            std::cout << "Added to floor " << currentFloor
                      << " task list, size=" << list.size() << std::endl;
            curPose.printInfo();
            break;
        }

        case 'd':
            taskThreadRun();
            break;

        case 'f':
        {
            if (currentFloor == 0)
            {
                poseList_f1.clear();
                poseList_f2.clear();
                std::cout << "Cleared BOTH task lists (no floor loaded)" << std::endl;
            }
            else
            {
                auto &list = (currentFloor == 1) ? poseList_f1 : poseList_f2;
                list.clear();
                std::cout << "Cleared floor " << currentFloor << " task list" << std::endl;
            }
            break;
        }

        case 'z':
            pauseNavFun();
            break;

        case 'x':
            resumeNavFun();
            break;

        case 'c':
            climbStairsFun();
            break;

        case 'S':
            saveTaskListFun();
            break;

        default:
            taskThreadStop();
            stopNodeFun();
            break;
        }
    }
}

int main(int argc, const char **argv)
{
    if (argc < 2)
    {
        std::cout << "Usage: " << argv[0] << " networkInterface" << std::endl;
        exit(-1);
    }
    unitree::robot::ChannelFactory::Instance()->Init(0, argv[1]);
    unitree::robot::slam::TestClient tc;

    tc.Init();
    tc.SetTimeout(60.0f);

    // Auto-load persisted task lists from f1.json / f2.json if they exist,
    // so that a demo run does not require pressing 's' again.
    tc.loadTaskListFun();

    tc.keyExecute();
    return 0;
}
