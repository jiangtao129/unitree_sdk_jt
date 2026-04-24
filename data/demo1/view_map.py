#!/usr/bin/env python3
"""
查看 Unitree SLAM 生成的 PCD 地图，并叠加同目录下 f*.json 任务列表里的航点。

可视化效果:
    - 点云按 Z 高度上色（蓝→绿→红 渐变，类似 turbo 配色）
    - 航点用球体表示: 绿球=起点, 红球=终点, 黄球=中间点
    - 相邻航点之间用橙色折线连接（按记录顺序）
    - 原点 (0,0,0) 处画一个 1m 长度的 RGB 三轴坐标系（X 红 / Y 绿 / Z 蓝）

操作 (open3d 默认窗口):
    左键拖动      旋转视角
    Shift + 左键  平移视角
    滚轮          缩放
    H             显示完整快捷键列表
    Q / 关闭窗口  退出
"""
from __future__ import annotations

# ---------------------------------------------------------------------------
# 重要: 下面这段 stub 必须在 `import open3d` 之前运行!
#
# 背景:
#   open3d 0.19 的 __init__.py 会无条件执行 `import open3d.ml`,
#   它会链式拉起 sklearn -> scipy. 本机的 scipy 是 apt 装的旧版
#   (按 numpy<1.25 的 ABI 编译), 而当前 numpy 是 2.2.6, 于是会触发
#   "numpy.core.multiarray failed to import" 的 ABI 报错, 让整个
#   import open3d 失败.
#
# 思路:
#   可视化用不到 open3d.ml, 所以提前往 sys.modules 里塞几个空 stub
#   占位, 让 open3d 自己执行 `import open3d.ml` 时直接命中假模块,
#   不再去拉 sklearn/scipy. open3d 后面的 _insert_pybind_names()
#   遍历到的就是个空模块, 啥也不做即可.
#
# 优点: 不动机器上任何包, 不需要 sudo / 不需要降级 numpy / 不需要 venv.
# ---------------------------------------------------------------------------
import sys
import types


def _install_open3d_ml_stubs() -> None:
    """提前给 open3d.ml 及其子模块安装空 stub，绕开 ABI 冲突。"""
    stub_names = [
        "open3d.ml",
        "open3d.ml.contrib",
        "open3d.ml.datasets",
        "open3d.ml.utils",
        "open3d.ml.vis",
        "open3d._ml3d",
        "open3d._ml3d.datasets",
        "open3d._ml3d.datasets.semantickitti",
    ]
    for name in stub_names:
        if name in sys.modules:
            continue
        mod = types.ModuleType(name)
        # 设置 __path__ 让 Python 把 stub 视作 package, 这样
        # `from open3d.ml import xxx` 之类的子模块查找不会再去硬盘
        # 找 .py 文件, 而是直接命中 sys.modules 里的 stub.
        mod.__path__ = []
        sys.modules[name] = mod


_install_open3d_ml_stubs()

import json  # noqa: E402
from pathlib import Path  # noqa: E402

import numpy as np  # noqa: E402
import open3d as o3d  # noqa: E402


HERE = Path(__file__).resolve().parent


def load_waypoints(json_path: Path) -> np.ndarray:
    """从 keyDemo3 保存的 f*.json 中读取航点列表，返回 N×3 (x,y,z) 数组。"""
    data = json.loads(json_path.read_text())
    if not data:
        return np.zeros((0, 3))
    return np.array([[p["x"], p["y"], p["z"]] for p in data], dtype=np.float64)


def make_sphere(center, radius, color):
    """生成一个指定位置/半径/颜色的实心球，用于标记航点。"""
    m = o3d.geometry.TriangleMesh.create_sphere(radius=radius, resolution=20)
    m.translate(np.asarray(center, dtype=np.float64))
    m.paint_uniform_color(color)
    m.compute_vertex_normals()
    return m


def make_line_strip(points: np.ndarray, color):
    """把若干个航点按顺序首尾相连，生成一条折线 LineSet。"""
    if len(points) < 2:
        return None
    ls = o3d.geometry.LineSet()
    ls.points = o3d.utility.Vector3dVector(points)
    ls.lines = o3d.utility.Vector2iVector(
        [[i, i + 1] for i in range(len(points) - 1)]
    )
    ls.colors = o3d.utility.Vector3dVector([color] * (len(points) - 1))
    return ls


def color_by_height(pc: o3d.geometry.PointCloud) -> None:
    """按点云 Z 高度生成渐变颜色（蓝→绿→红），原地写回 pc.colors。"""
    pts = np.asarray(pc.points)
    if pts.size == 0:
        return
    z = pts[:, 2]
    # 取 2%~98% 分位作为色阶范围, 避免极端高/低点把整体色阶压扁
    z_lo, z_hi = np.percentile(z, 2), np.percentile(z, 98)
    if z_hi - z_lo < 1e-3:
        z_hi = z_lo + 1e-3
    t = np.clip((z - z_lo) / (z_hi - z_lo), 0.0, 1.0)
    # 蓝 -> 绿 -> 红 渐变 (类 turbo 配色)
    r = np.clip(1.5 - np.abs(4 * t - 3), 0.0, 1.0)
    g = np.clip(1.5 - np.abs(4 * t - 2), 0.0, 1.0)
    b = np.clip(1.5 - np.abs(4 * t - 1), 0.0, 1.0)
    pc.colors = o3d.utility.Vector3dVector(np.stack([r, g, b], axis=1))


def build_geometries(pcd_path: Path, json_path: Path | None):
    """读取 PCD + 航点 JSON, 组装出 open3d 能直接绘制的几何体列表。

    返回:
        geoms: 几何体列表 (PCD + 坐标轴 + 若干航点球 + 折线)
        pc:    PCD 点云对象 (主调用方还需要它来取 bbox)
    """
    pc = o3d.io.read_point_cloud(str(pcd_path))
    if len(pc.points) == 0:
        raise RuntimeError(f"empty pcd: {pcd_path}")
    print(f"[info] loaded {pcd_path} : {len(pc.points)} points")
    color_by_height(pc)

    # 默认放上原点坐标轴, 方便目测尺度和方向
    geoms = [pc, o3d.geometry.TriangleMesh.create_coordinate_frame(size=1.0)]

    if json_path and json_path.exists():
        wps = load_waypoints(json_path)
        print(f"[info] loaded {json_path} : {len(wps)} waypoints")
        n = len(wps)
        for i, p in enumerate(wps):
            if i == 0:
                color = [0.1, 0.9, 0.1]      # 起点: 绿
            elif i == n - 1:
                color = [0.95, 0.2, 0.2]     # 终点: 红
            else:
                color = [1.0, 0.85, 0.1]     # 中间: 黄
            geoms.append(make_sphere(p, radius=0.25, color=color))
        ls = make_line_strip(wps, color=[1.0, 0.4, 0.0])
        if ls is not None:
            geoms.append(ls)
    elif json_path:
        print(f"[warn] no waypoint file at {json_path}, showing PCD only")

    return geoms, pc


def resolve_paths(argv):
    """根据命令行参数推导 PCD 与 JSON 的实际路径。

    规则:
        - 不传参 -> floor1.pcd + f1.json
        - 只传 PCD, 文件名以 1.pcd / 2.pcd 结尾 -> 自动配对 f1.json / f2.json
        - 传两个参数 -> 完全按用户给的来
        - 相对路径相对于本脚本所在目录 (HERE)
    """
    pcd_arg = argv[1] if len(argv) >= 2 else "floor1.pcd"
    if len(argv) >= 3:
        json_arg = argv[2]
    elif pcd_arg.endswith("1.pcd"):
        json_arg = "f1.json"
    elif pcd_arg.endswith("2.pcd"):
        json_arg = "f2.json"
    else:
        json_arg = None
    pcd_path = (HERE / pcd_arg) if not Path(pcd_arg).is_absolute() else Path(pcd_arg)
    json_path = None
    if json_arg:
        json_path = (HERE / json_arg) if not Path(json_arg).is_absolute() else Path(json_arg)
    return pcd_path, json_path


def selftest() -> int:
    """无窗口自测: 验证 stub 生效 + PCD/JSON 能正常加载, 适合 CI / SSH 环境。"""
    print("[selftest] open3d", o3d.__version__, "numpy", np.__version__)
    pcd_path, json_path = resolve_paths(["view_map.py"])
    geoms, pc = build_geometries(pcd_path, json_path)
    bbox = pc.get_axis_aligned_bounding_box()
    extent = bbox.get_extent()
    print(f"[selftest] bbox extent (m) = "
          f"x:{extent[0]:.2f}  y:{extent[1]:.2f}  z:{extent[2]:.2f}")
    print(f"[selftest] geometry count = {len(geoms)}")
    print("[selftest] OK")
    return 0


def main() -> int:
    """主流程: 解析参数 -> 构建几何体 -> 弹窗显示 (除非 --selftest)。"""
    if "--selftest" in sys.argv:
        return selftest()

    pcd_path, json_path = resolve_paths(sys.argv)
    geoms, _ = build_geometries(pcd_path, json_path)

    o3d.visualization.draw_geometries(
        geoms,
        window_name=f"{pcd_path.name}  |  drag=rotate  shift+drag=pan  wheel=zoom",
        width=1280,
        height=800,
    )
    return 0


# ---------------------------------------------------------------------------
# 终端调用命令 (在 data/demo1/ 目录下执行):
#
#   # 看一楼地图 + 一楼航点 (默认)
#   python3 view_map.py
#
#   # 看二楼地图 + 二楼航点
#   python3 view_map.py floor2.pcd f2.json
#
#   # 只看 PCD 不叠加航点 (传一个不带 1.pcd / 2.pcd 后缀的文件名即可)
#   python3 view_map.py floor1.pcd none.json
#
#   # 不弹窗的自测 (用于 SSH 终端 / 没有 X11 显示的环境)
#   python3 view_map.py --selftest
#
# 也可以从任意目录用绝对路径调用:
#   python3 /home/jiangtao/huazhijian/project/old_go2/unitree_sdk2/data/demo1/view_map.py
# ---------------------------------------------------------------------------
if __name__ == "__main__":
    sys.exit(main())
