#include <unitree/robot/client/client.hpp>
#include <unitree/robot/channel/channel_subscriber.hpp>
#include <unitree/robot/go2/sport/sport_client.hpp>
#include <unitree/idl/ros2/String_.hpp>
#include <unitree/idl/go2/SportModeState_.hpp>
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
#include <cmath>
#include <chrono>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <streambuf>
#include <ctime>

// TeeBuf duplicates every byte written to std::cout into a second streambuf
// (typically a log file's). We install it in main() so that everything the
// terminal shows is also persisted to keyDemo3_YYYYMMDD_HHMMSS.log in cwd.
// Marked final: this class is a leaf, no part of the codebase needs to extend
// it; making it final lets the compiler devirtualize sync() / overflow()
// calls and prevents accidental subclassing that could break the contract.
class TeeBuf final : public std::streambuf
{
public:
    TeeBuf(std::streambuf *a, std::streambuf *b) : a_(a), b_(b) {}

protected:
    int overflow(int c) override
    {
        if (c == EOF)
            return !EOF;
        int r1 = a_->sputc(static_cast<char>(c));
        int r2 = b_->sputc(static_cast<char>(c));
        return (r1 == EOF || r2 == EOF) ? EOF : c;
    }
    int sync() override
    {
        int r1 = a_->pubsync();
        int r2 = b_->pubsync();
        return (r1 == 0 && r2 == 0) ? 0 : -1;
    }

private:
    std::streambuf *a_;
    std::streambuf *b_;
};

#define SlamInfoTopic "rt/slam_info"
#define SlamKeyInfoTopic "rt/slam_key_info"
#define SportModeStateTopic "rt/sportmodestate"

using namespace unitree::robot;
using namespace unitree::common;

// Wrap angle to [-pi, pi] in a numerically-robust way (handles any multiples of 2*pi).
static inline float wrapPi(float a)
{
    return std::atan2(std::sin(a), std::cos(a));
}

// Extract z-axis yaw from (qx, qy, qz, qw), result in radians in [-pi, pi].
static inline float yawFromQuat(float qx, float qy, float qz, float qw)
{
    float siny_cosp = 2.0f * (qw * qz + qx * qy);
    float cosy_cosp = 1.0f - 2.0f * (qy * qy + qz * qz);
    return std::atan2(siny_cosp, cosy_cosp);
}

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
        ChannelSubscriberPtr<unitree_go::msg::dds_::SportModeState_> subSportState;

        void slamInfoHandler(const void *message);
        void slamKeyInfoHandler(const void *message);
        void sportStateHandler(const void *message);

        poseDate curPose;

        // Per-floor task lists (inherits from keyDemo2 Phase 2).
        // Threading model: only the main keyExecute() loop reads/writes
        // these vectors (case 's' / 'S' / 'd' / 'f'). climbThread does NOT
        // touch them. slamInfoHandler / slamKeyInfoHandler (DDS callbacks)
        // do not touch them either. Therefore no mutex is required, but if
        // a future PR makes either climbThread or a DDS handler read these,
        // it MUST add either an std::mutex or migrate to a thread-safe
        // container.
        std::vector<poseDate> poseList_f1;
        std::vector<poseDate> poseList_f2;

        // Dirty flags: "memory differs from disk" for each floor's task list.
        // - s / f flip this to true (memory changed, json not yet updated)
        // - S (saveTaskListFun) writes to disk and flips it back to false
        // - d refuses to execute while dirty, forcing an explicit S save
        bool poseList_f1_dirty = false;
        bool poseList_f2_dirty = false;

        bool is_arrived = false;
        bool threadControl = false;
        std::future<void> futThread;
        std::promise<void> prom;
        std::thread controlThread;

        // Direct Go2 body control during blind stair climbing.
        unitree::robot::go2::SportClient sportClient;
        std::atomic<bool> is_climbing{false};
        std::thread climbThread;

        // Body-frame pose feedback from rt/sportmodestate. Used by the CTE
        // control loop; also tapped for the pre-align step.
        std::atomic<float> body_x{0.0f};
        std::atomic<float> body_y{0.0f};
        std::atomic<float> body_yaw{0.0f};
        std::atomic<bool> sport_state_valid{false};

    public:
        // Climb control tunables. Public so you can tweak and rebuild fast.
        // Hard upper bound on forward velocity actually fed to SportClient::Move.
        // Even if climb_vx is mistakenly set higher (typo / runtime patch), the
        // clamp at the call site below keeps the dog inside the stairs-safe
        // envelope (~1.0 m/s for Go2 EDU on stairs).
        static constexpr float kClimbVxMax = 1.0f;
        // Climb control loop period: SportClient::Move is republished at
        // 1000/period_ms Hz. 20 ms = 50 Hz, the documented Go2 motion command
        // rate; lower than ~33 Hz the dog will auto-brake during the gap.
        static constexpr int kClimbLoopPeriodMs = 20;
        float climb_vx = 0.35f;        // constant forward velocity while climbing (m/s)
        float K_y = 0.5f;              // cross-track gain: dy 1 m -> desired_yaw offset 28.6 deg
        float max_yaw_offset = 0.52f;  // clamp on |desired_yaw - stair_yaw_body| (30 deg)
        float K_psi = 1.5f;            // P gain from yaw_err to vyaw
        float vyaw_max = 0.4f;         // |vyaw| clamp (rad/s)
        float align_limit = 0.0873f;   // pre-align max single-shot turn: 5 deg
        float align_tol = 0.03f;       // pre-align convergence tolerance: ~1.7 deg
        float align_timeout = 3.0f;    // pre-align hard timeout (s)

        // Map file paths and persistence targets.
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

        // CTE-based stair climbing toggle.
        void climbStairsFun();

        // Helpers.
        int chooseFloor(const char *prompt);
        void saveTaskListFun();
        void loadTaskListFun();
        int loadFloorListFromDisk(int floor);
    };

    TestClient::TestClient() : Client(TEST_SERVICE_NAME, false)
    {
        subSlamInfo = ChannelSubscriberPtr<std_msgs::msg::dds_::String_>(new ChannelSubscriber<std_msgs::msg::dds_::String_>(SlamInfoTopic));
        subSlamInfo->InitChannel(std::bind(&unitree::robot::slam::TestClient::slamInfoHandler, this, std::placeholders::_1), 1);
        subSlamKeyInfo = ChannelSubscriberPtr<std_msgs::msg::dds_::String_>(new ChannelSubscriber<std_msgs::msg::dds_::String_>(SlamKeyInfoTopic));
        subSlamKeyInfo->InitChannel(std::bind(&unitree::robot::slam::TestClient::slamKeyInfoHandler, this, std::placeholders::_1), 1);
        subSportState = ChannelSubscriberPtr<unitree_go::msg::dds_::SportModeState_>(
            new ChannelSubscriber<unitree_go::msg::dds_::SportModeState_>(SportModeStateTopic));
        subSportState->InitChannel(
            std::bind(&unitree::robot::slam::TestClient::sportStateHandler, this, std::placeholders::_1), 1);

        std::cout << "********************  Unitree SLAM Demo (keyDemo3) ***************\n";
        std::cout << "---------------            q    w                -----------------\n";
        std::cout << "---------------            a    s   d   f        -----------------\n";
        std::cout << "---------------            z    x   c   S        -----------------\n";
        std::cout << "------------------------------------------------------------------\n";
        std::cout << "------------------ q: Start mapping             ------------------\n";
        std::cout << "------------------ w: End mapping (pick 1 or 2) ------------------\n";
        std::cout << "------------------                + auto stopNode ----------------\n";
        std::cout << "------------------ a: Relocation   (pick 1 or 2)------------------\n";
        std::cout << "------------------ s: push pose to memory (dirty)-----------------\n";
        std::cout << "------------------ d: reload + execute (SAVE 1st)-----------------\n";
        std::cout << "------------------ f: Clear current floor (dirty)-----------------\n";
        std::cout << "------------------ z: Pause navigation          ------------------\n";
        std::cout << "------------------ x: Resume navigation         ------------------\n";
        std::cout << "------------------ c: Climb stairs TOGGLE (CTE) ------------------\n";
        std::cout << "------------------ S: SAVE memory -> json (abs) ------------------\n";
        std::cout << "---------------- Press any other key to stop SLAM ----------------\n";
        std::cout << "------------------------------------------------------------------\n";
        std::cout << "--------------- Press 'Ctrl + C' to exit the program -------------\n";
        std::cout << "------------------------------------------------------------------\n"
                  << std::endl;
    }

    TestClient::~TestClient()
    {
        // Destructors are noexcept by default (C++11+). Anything that throws
        // here would call std::terminate. The DDS / SportClient APIs are not
        // exception-safe by contract, so wrap them defensively to make the
        // shutdown path robust against an exception from any one call still
        // letting the rest run.
        try
        {
            if (is_climbing.load())
            {
                is_climbing.store(false);
                if (climbThread.joinable())
                    climbThread.join();
                sportClient.StopMove();
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "[~TestClient] climb-shutdown threw: " << e.what() << std::endl;
        }
        catch (...)
        {
            std::cerr << "[~TestClient] climb-shutdown threw unknown" << std::endl;
        }
        try
        {
            stopNodeFun();
        }
        catch (const std::exception &e)
        {
            std::cerr << "[~TestClient] stopNodeFun threw: " << e.what() << std::endl;
        }
        catch (...)
        {
            std::cerr << "[~TestClient] stopNodeFun threw unknown" << std::endl;
        }
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
    // Guard against malformed JSON from slam_server. Without this catch a
    // parse_error would unwind out of the DDS callback thread and abort the
    // whole process; we'd rather log + drop the bad packet.
    nlohmann::json jsonData;
    try
    {
        jsonData = nlohmann::json::parse(currentMsg.data());
    }
    catch (const nlohmann::json::parse_error &e)
    {
        std::cerr << "[slamInfoHandler] dropping malformed JSON: "
                  << e.what() << std::endl;
        return;
    }

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
    // Same protection as slamInfoHandler above; see comment there.
    nlohmann::json jsonData;
    try
    {
        jsonData = nlohmann::json::parse(currentMsg.data());
    }
    catch (const nlohmann::json::parse_error &e)
    {
        std::cerr << "[slamKeyInfoHandler] dropping malformed JSON: "
                  << e.what() << std::endl;
        return;
    }

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

void unitree::robot::slam::TestClient::sportStateHandler(const void *message)
{
    const unitree_go::msg::dds_::SportModeState_ &st =
        *(const unitree_go::msg::dds_::SportModeState_ *)message;
    body_x.store(st.position()[0]);
    body_y.store(st.position()[1]);
    body_yaw.store(st.imu_state().rpy()[2]);
    sport_state_valid.store(true);
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
    if (is_climbing.load())
    {
        // Second press: stop the CTE follower and hand control back to slam.
        is_climbing.store(false);
        if (climbThread.joinable())
            climbThread.join();
        sportClient.StopMove();
        sportClient.BalanceStand();
        resumeNavFun();
        std::cout << "\033[1;36m"
                  << ">>> Climb stopped by user. Dog is in BalanceStand, SLAM resumed."
                  << "\033[0m" << std::endl;
        return;
    }

    // --- First press: start CTE climb ---

    // Step 1: hand off control from the SLAM planner and let inertia decay.
    pauseNavFun();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    sportClient.BalanceStand();
    std::this_thread::sleep_for(std::chrono::milliseconds(400));

    if (!sport_state_valid.load())
    {
        std::cout << "\033[1;31m"
                  << "[climb] WARN: No rt/sportmodestate samples received yet. "
                  << "CTE feedback will be garbage; aborting. "
                  << "Check DDS network, then press 'c' again."
                  << "\033[0m" << std::endl;
        resumeNavFun();
        return;
    }

    // Step 2: determine stair_yaw in the SLAM frame.
    // Priority 1: yaw from the last task-list point of floor1 (you record the
    //             stair-entrance point as "facing the stairs").
    // Priority 2: curPose yaw (fallback when no task list is present).
    float stair_yaw_slam;
    if (!poseList_f1.empty())
    {
        const auto &p = poseList_f1.back();
        stair_yaw_slam = yawFromQuat(p.q_x, p.q_y, p.q_z, p.q_w);
        std::cout << "[climb] stair_yaw_slam from poseList_f1.back(): "
                  << stair_yaw_slam << " rad ("
                  << stair_yaw_slam * 180.0f / static_cast<float>(M_PI) << " deg)"
                  << std::endl;
    }
    else
    {
        stair_yaw_slam = yawFromQuat(curPose.q_x, curPose.q_y, curPose.q_z, curPose.q_w);
        std::cout << "[climb] (fallback) stair_yaw_slam from curPose: "
                  << stair_yaw_slam << " rad ("
                  << stair_yaw_slam * 180.0f / static_cast<float>(M_PI) << " deg)"
                  << std::endl;
    }

    // Step 3: map stair_yaw into the body odom frame.
    // delta_yaw captures the static yaw offset between the two frames at t0;
    // it is assumed constant during the climb.
    float yaw0_body = body_yaw.load();
    float yaw0_slam = yawFromQuat(curPose.q_x, curPose.q_y, curPose.q_z, curPose.q_w);
    float delta_yaw = wrapPi(yaw0_body - yaw0_slam);
    float stair_yaw_body_local = wrapPi(stair_yaw_slam + delta_yaw);
    std::cout << "[climb] body_yaw(t0)=" << yaw0_body
              << ", curPose.yaw(t0)=" << yaw0_slam
              << ", delta_yaw=" << delta_yaw
              << ", stair_yaw_body=" << stair_yaw_body_local << std::endl;

    // Step 4: pre-align with a ±align_limit clamp. We only turn at most
    // align_limit radians in this one shot; any residual error is handled
    // by the CTE main loop. The reference direction *stays* stair_yaw_body.
    float real_err = wrapPi(stair_yaw_body_local - yaw0_body);
    float clipped_err = std::clamp(real_err, -align_limit, align_limit);
    float align_target = wrapPi(yaw0_body + clipped_err);
    std::cout << "[climb] pre-align: real_err=" << real_err
              << " rad, clipped_err=" << clipped_err
              << " rad, target=" << align_target << " rad" << std::endl;

    auto align_start = std::chrono::steady_clock::now();
    while (true)
    {
        float yerr = wrapPi(align_target - body_yaw.load());
        if (std::fabs(yerr) < align_tol)
            break;
        float elapsed = std::chrono::duration<float>(
                            std::chrono::steady_clock::now() - align_start)
                            .count();
        if (elapsed > align_timeout)
        {
            std::cout << "[climb] pre-align timeout, residual yerr=" << yerr
                      << " rad; entering main loop anyway." << std::endl;
            break;
        }
        float vyaw = std::clamp(K_psi * yerr, -vyaw_max, vyaw_max);
        sportClient.Move(0.0f, 0.0f, vyaw);
        std::this_thread::sleep_for(std::chrono::milliseconds(kClimbLoopPeriodMs));
    }

    // Step 5: lock P_start AFTER pre-align so that dy=0 at t=0 corresponds to
    // a settled, aligned body pose.
    float x0 = body_x.load();
    float y0 = body_y.load();
    float cos_s = std::cos(stair_yaw_body_local);
    float sin_s = std::sin(stair_yaw_body_local);
    std::cout << "\033[1;36m"
              << ">>> Climb started. P_start=(" << x0 << ", " << y0
              << "), stair_yaw_body=" << stair_yaw_body_local << " rad."
              << " K_y=" << K_y << ", K_psi=" << K_psi
              << ", climb_vx=" << climb_vx << ". Press 'c' again to stop."
              << "\033[0m" << std::endl;

    // Step 6: fire up the 50 Hz CTE control loop. vx is constant. No auto-stop.
    is_climbing.store(true);
    climbThread = std::thread([this, x0, y0, cos_s, sin_s, stair_yaw_body_local]() {
        int log_cnt = 0;
        while (is_climbing.load())
        {
            float xc = body_x.load();
            float yc = body_y.load();
            float yawc = body_yaw.load();

            float dpx = xc - x0;
            float dpy = yc - y0;
            float dx_path = dpx * cos_s + dpy * sin_s;
            float dy_path = -dpx * sin_s + dpy * cos_s;

            float raw_offset = -K_y * dy_path;
            float offset = std::clamp(raw_offset, -max_yaw_offset, max_yaw_offset);
            float desired_yaw = wrapPi(stair_yaw_body_local + offset);
            float yerr = wrapPi(desired_yaw - yawc);
            float vyaw = std::clamp(K_psi * yerr, -vyaw_max, vyaw_max);

            // Safety clamp on forward velocity before issuing the motion command.
            // climb_vx is a public tunable; without this clamp a typo could push
            // the dog above the stairs-safe envelope (kClimbVxMax = 1.0 m/s).
            sportClient.Move(std::clamp(climb_vx, 0.0f, kClimbVxMax), 0.0f, vyaw);

            if ((++log_cnt % 25) == 0)
            {
                std::cout << "[climb] dx=" << dx_path
                          << " dy=" << dy_path
                          << " offs=" << offset
                          << " yerr=" << yerr
                          << " vyaw=" << vyaw << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(kClimbLoopPeriodMs));
        }
    });
}

int unitree::robot::slam::TestClient::chooseFloor(const char *prompt)
{
    // keyDetection() only flips the terminal to non-canonical during one
    // getchar(). By the time we get here we are back in canonical mode; we
    // flip explicitly anyway to be safe against stale state.
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
        // Atomic write: serialize the full JSON to "<path>.tmp" first, ensure
        // the bytes are flushed to disk, then std::filesystem::rename onto the
        // real path. rename(2) on the same filesystem is atomic at the inode
        // level, so a crash mid-write leaves the previous f*.json intact
        // instead of a half-written / empty file. This protects multi-floor
        // task lists during long demo runs from a single SIGKILL or kernel
        // panic destroying the user's recorded waypoints.
        const std::string tmp = path + ".tmp";
        {
            std::ofstream f(tmp);
            if (!f.is_open())
                return false;
            f << arr.dump(2);
            if (!f.good())
                return false;
        } // ofstream destructor flushes + closes before rename below
        std::error_code ec;
        std::filesystem::rename(tmp, path, ec);
        if (ec)
        {
            std::filesystem::remove(tmp, ec);
            return false;
        }
        return true;
    };

    bool ok1 = dumpList(poseList_f1, taskListPath_f1);
    bool ok2 = dumpList(poseList_f2, taskListPath_f2);

    // Resolve to absolute paths so you can paste them into other shells.
    std::string abs1, abs2;
    try
    {
        abs1 = std::filesystem::absolute(taskListPath_f1).string();
    }
    catch (...)
    {
        abs1 = taskListPath_f1;
    }
    try
    {
        abs2 = std::filesystem::absolute(taskListPath_f2).string();
    }
    catch (...)
    {
        abs2 = taskListPath_f2;
    }

    std::cout << "[S] floor1 (" << poseList_f1.size() << " pts) -> "
              << abs1 << (ok1 ? " [OK]" : " [FAIL]") << std::endl;
    std::cout << "[S] floor2 (" << poseList_f2.size() << " pts) -> "
              << abs2 << (ok2 ? " [OK]" : " [FAIL]") << std::endl;

    // Only flip dirty off when the corresponding file actually succeeded.
    if (ok1)
        poseList_f1_dirty = false;
    if (ok2)
        poseList_f2_dirty = false;
}

// Load one floor's task list from its json file, overwriting the in-memory
// poseList_fN. Returns the number of points loaded on success, -1 if the
// file cannot be opened, -2 on parse error, -3 on bad floor arg. After
// loading, dirty is cleared because memory == disk.
int unitree::robot::slam::TestClient::loadFloorListFromDisk(int floor)
{
    if (floor != 1 && floor != 2)
        return -3;
    const std::string &path = (floor == 1) ? taskListPath_f1 : taskListPath_f2;
    std::vector<poseDate> &list = (floor == 1) ? poseList_f1 : poseList_f2;

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

    if (floor == 1)
        poseList_f1_dirty = false;
    else
        poseList_f2_dirty = false;
    return static_cast<int>(list.size());
}

void unitree::robot::slam::TestClient::loadTaskListFun()
{
    int n1 = loadFloorListFromDisk(1);
    int n2 = loadFloorListFromDisk(2);
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
            if (idx == 1 || idx == 2)
            {
                endMappingPlFun(idx == 1 ? floor1_pcd : floor2_pcd);
                // Critical: tear down the slam node so the next 'q' is a
                // clean mapping session. Without this, FAST-LIO reuses its
                // local cube (300 m) from the previous session and the dog
                // triggers "Exceeding the maximum value range" when we try
                // to start mapping after moving to the next floor.
                stopNodeFun();
                std::this_thread::sleep_for(std::chrono::milliseconds(1500));
                currentFloor = 0;
                std::cout << "\033[1;36m"
                          << ">>> Mapping session closed. Press 'q' to start fresh, "
                             "or 'a' to load a map."
                          << "\033[0m" << std::endl;
            }
            else
            {
                std::cout << "Invalid choice, expected 1 or 2" << std::endl;
            }
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
                // relocation). Stop it cleanly before starting a new
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
            if (currentFloor == 1)
                poseList_f1_dirty = true;
            else
                poseList_f2_dirty = true;
            std::cout << "Added to floor " << currentFloor
                      << " task list (memory), size=" << list.size()
                      << ". dirty=true, press 'S' to persist." << std::endl;
            curPose.printInfo();
            break;
        }

        case 'd':
        {
            if (currentFloor == 0)
            {
                std::cout << "No map loaded; press 'a' first to pick a floor." << std::endl;
                break;
            }
            bool dirty = (currentFloor == 1) ? poseList_f1_dirty : poseList_f2_dirty;
            int memSize = static_cast<int>(
                (currentFloor == 1) ? poseList_f1.size() : poseList_f2.size());
            if (dirty)
            {
                std::cout << "\033[1;31m"
                          << "[d] Floor " << currentFloor << " has UNSAVED changes in memory ("
                          << memSize << " pts). Press 'S' to save first, or 'f' to discard."
                          << "\033[0m" << std::endl;
                break;
            }
            // Not dirty: reload from disk to guarantee we execute exactly what
            // was last saved (someone might have edited the json by hand).
            int n = loadFloorListFromDisk(currentFloor);
            if (n < 0)
            {
                std::cout << "[d] Failed to reload f" << currentFloor
                          << ".json (code=" << n << "). Aborting." << std::endl;
                break;
            }
            std::cout << "[d] Reloaded floor " << currentFloor << " (" << n
                      << " pts) from disk, starting navigation..." << std::endl;
            taskThreadRun();
            break;
        }

        case 'f':
        {
            if (currentFloor == 0)
            {
                poseList_f1.clear();
                poseList_f2.clear();
                poseList_f1_dirty = true;
                poseList_f2_dirty = true;
                std::cout << "Cleared BOTH task lists (memory). dirty=true for both, "
                             "press 'S' to overwrite json."
                          << std::endl;
            }
            else
            {
                auto &list = (currentFloor == 1) ? poseList_f1 : poseList_f2;
                list.clear();
                if (currentFloor == 1)
                    poseList_f1_dirty = true;
                else
                    poseList_f2_dirty = true;
                std::cout << "Cleared floor " << currentFloor
                          << " task list (memory). dirty=true, press 'S' to overwrite json."
                          << std::endl;
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
        // exit(-1) gets truncated to 255 by 8-bit POSIX exit-code semantics,
        // hiding "missing arg" behind the same code as a segfault. Use
        // EXIT_FAILURE (= 1) and return so main can unwind cleanly.
        return EXIT_FAILURE;
    }

    // Tee std::cout to a timestamped log file in the current working
    // directory. Keeping logFile + tee in main's scope ensures they outlive
    // the keyExecute() loop; when the user Ctrl+C's, the OS flushes the
    // ofstream on exit.
    std::time_t now_t = std::time(nullptr);
    std::tm tm_info;
    localtime_r(&now_t, &tm_info);
    char logname[96];
    std::strftime(logname, sizeof(logname), "keyDemo3_%Y%m%d_%H%M%S.log", &tm_info);
    std::ofstream logFile(logname);
    std::streambuf *origCout = std::cout.rdbuf();
    TeeBuf tee(origCout, logFile.rdbuf());
    if (logFile.is_open())
    {
        std::cout.rdbuf(&tee);
        std::string absLog;
        try
        {
            absLog = std::filesystem::absolute(logname).string();
        }
        catch (...)
        {
            absLog = logname;
        }
        std::cout << "=== keyDemo3 started, log: " << absLog << " ===" << std::endl;
    }
    else
    {
        std::cerr << "[warn] cannot open " << logname
                  << ", running without file log" << std::endl;
    }

    unitree::robot::ChannelFactory::Instance()->Init(0, argv[1]);
    unitree::robot::slam::TestClient tc;

    tc.Init();
    tc.SetTimeout(60.0f);

    // Auto-load persisted task lists so demo runs do not require re-pressing 's'.
    tc.loadTaskListFun();

    tc.keyExecute();
    // Restore std::cout before main returns, in case anything prints during
    // destruction and logFile is gone.
    std::cout.rdbuf(origCout);
    return 0;
}
