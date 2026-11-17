"""边界测试：构造包含 0~255（Byte）与 0~65535（UInt16）的小图像，运行 ImageTest，检查减 10 后小于 0 的像素被置 0。"""
import subprocess, sys, os, json
import numpy as np
from osgeo import gdal
gdal.UseExceptions()
exe = sys.argv[1] if len(sys.argv) > 1 else "build/ImageTest"
tmp = "results/edge_tmp"; os.makedirs(tmp, exist_ok=True)
results = {}
for dtype, gdt, vmax in [(np.uint8, gdal.GDT_Byte, 255), (np.uint16, gdal.GDT_UInt16, 65535)]:
    name = gdal.GetDataTypeName(gdt)
    rng = np.random.default_rng(0)
    arr = rng.integers(0, vmax + 1, size=(2, 37, 53), dtype=np.int64).astype(dtype)
    arr[:, 0, :12] = np.arange(12)          # 显式包含 0..11
    arr[:, 1, 0] = vmax
    src = f"{tmp}/in_{name}.tif"; dst = f"{tmp}/out_{name}.tif"
    ds = gdal.GetDriverByName("GTiff").Create(src, 53, 37, 2, gdt)
    for i in range(2): ds.GetRasterBand(i + 1).WriteArray(arr[i])
    ds = None
    log = subprocess.run([exe, src, dst, "10"], capture_output=True, text=True, check=True).stdout
    out = gdal.Open(dst).ReadAsArray()
    exp = np.clip(arr.astype(np.int64) - 10, 0, None).astype(dtype)
    results[name] = {"values": int(arr.size), "below_10": int((arr < 10).sum()),
                     "mismatched": int((out != exp).sum()), "PASS": bool((out == exp).all()),
                     "program_log": log.strip().splitlines()}
print(json.dumps(results, indent=2, ensure_ascii=False))
json.dump(results, open("results/edge_test_report.json", "w"), indent=2, ensure_ascii=False)
for f in os.listdir(tmp): os.remove(os.path.join(tmp, f))
os.rmdir(tmp)
sys.exit(0 if all(r["PASS"] for r in results.values()) else 1)
