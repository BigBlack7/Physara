"""Golden-image 视觉回归比对工具(change pre-refactor-validation-guards / P.2)。

用法
  比对:  python Tools/compare_golden.py <candidate.png> <golden.png>
                [--tol 8] [--fail-ratio 0.002] [--diff <out.png>]
          差异在容差内 -> exit 0;超出 -> exit 1 并打印指标(可选输出 diff 图)。

  刷新:  python Tools/compare_golden.py --update <golden.png> <candidate.png>
                --reason "为何有意改变画面"
          有意画面变更(如 reverse-Z/材质布局重设计)时覆盖 golden,并把原因
          追加到 golden 同目录的 GOLDEN_REASONS.log,禁止无记录的静默覆盖。

比对判据(D4/容差法,非逐像素精确):
  对每个像素取各通道绝对差的最大值,> tol 记为"失败像素";
  失败像素占比 > fail-ratio 判定回归失败。
依赖:Pillow 与 numpy(缺失时 fail-loud 并提示安装)。
"""

from datetime import datetime
from pathlib import Path
import argparse
import sys


def _load_deps():
    try:
        import numpy as np  # noqa: F401
        from PIL import Image  # noqa: F401
    except ImportError as exc:
        print(
            "compare_golden 需要 Pillow 与 numpy:pip install pillow numpy\n"
            f"缺失:{exc}",
            file=sys.stderr,
        )
        sys.exit(2)
    return np, Image


def _read_rgba(path, np, Image):
    file = Path(path)
    if not file.is_file():
        print(f"图像不存在:{file}", file=sys.stderr)
        sys.exit(2)
    image = Image.open(file).convert("RGBA")
    return np.asarray(image, dtype=np.int16)


def compare(candidate, golden, tol, fail_ratio, diff_path):
    np, Image = _load_deps()
    cand = _read_rgba(candidate, np, Image)
    gold = _read_rgba(golden, np, Image)

    if cand.shape != gold.shape:
        print(f"尺寸不一致:candidate={cand.shape} golden={gold.shape}", file=sys.stderr)
        return 1

    per_pixel_max = np.max(np.abs(cand - gold), axis=2)
    failed = int(np.count_nonzero(per_pixel_max > tol))
    total = int(per_pixel_max.size)
    ratio = failed / total if total else 0.0
    max_diff = int(per_pixel_max.max()) if total else 0
    mean_diff = float(per_pixel_max.mean()) if total else 0.0

    print(
        f"max_diff={max_diff} mean_diff={mean_diff:.3f} "
        f"failed_pixels={failed}/{total} ({ratio:.4%}) "
        f"tol={tol} fail_ratio={fail_ratio:.4%}"
    )

    if diff_path:
        heat = np.clip(per_pixel_max * 4, 0, 255).astype("uint8")
        Image.fromarray(heat, mode="L").save(diff_path)
        print(f"diff 图已写入:{diff_path}")

    if ratio > fail_ratio:
        print("视觉回归:FAIL(差异超容差)", file=sys.stderr)
        return 1
    print("视觉回归:PASS")
    return 0


def update(golden, candidate, reason):
    import shutil

    golden_path = Path(golden)
    candidate_path = Path(candidate)
    if not candidate_path.is_file():
        print(f"candidate 不存在:{candidate_path}", file=sys.stderr)
        return 2
    golden_path.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(candidate_path, golden_path)

    log = golden_path.parent / "GOLDEN_REASONS.log"
    stamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    with log.open("a", encoding="utf-8") as handle:
        handle.write(f"[{stamp}] {golden_path.name}: {reason}\n")
    print(f"已刷新 golden:{golden_path};原因记入 {log}")
    return 0


def main():
    parser = argparse.ArgumentParser(description="Golden-image 视觉回归比对")
    parser.add_argument("--update", action="store_true", help="刷新 golden 模式")
    parser.add_argument("--reason", default="", help="刷新 golden 的原因(--update 必填)")
    parser.add_argument("--tol", type=int, default=8, help="单通道绝对差阈值")
    parser.add_argument("--fail-ratio", type=float, default=0.002, help="允许失败像素占比")
    parser.add_argument("--diff", default="", help="可选:diff 热力图输出路径")
    parser.add_argument("first")
    parser.add_argument("second")
    args = parser.parse_args()

    if args.update:
        if not args.reason.strip():
            print("--update 必须提供 --reason(禁止无记录静默覆盖)", file=sys.stderr)
            sys.exit(2)
        # 刷新:first=golden(目标),second=candidate(新图)
        sys.exit(update(args.first, args.second, args.reason.strip()))

    # 比对:first=candidate,second=golden
    sys.exit(compare(args.first, args.second, args.tol, args.fail_ratio, args.diff or None))


if __name__ == "__main__":
    main()
