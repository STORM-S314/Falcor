import os
import pyexr
from PIL import Image
import numpy as np
from concurrent.futures import ThreadPoolExecutor, as_completed
from tqdm import tqdm

def convert_exr_to_png_and_delete(exr_path, png_path):
    try:
        exr_image = pyexr.open(exr_path)
        exr_data = exr_image.get("default")
        exr_data = np.clip(exr_data, 0, 1)  # 将数据裁剪到[0, 1]范围
        exr_data = (exr_data * 255).astype(np.uint8)  # 转换为uint8格式
        png_image = Image.fromarray(exr_data)
        png_image.save(png_path)
        # 删除原始的EXR文件
        os.remove(exr_path)
    except Exception as e:
        print(f"Error processing {exr_path}: {e}")

def convert_all_exr_to_png_and_delete(directory, max_workers=4):
    if not os.path.exists(directory):
        print(f"Directory {directory} does not exist.")
        return

    exr_files = [os.path.join(directory, filename) for filename in os.listdir(directory) if filename.lower().endswith('.exr')]

    with ThreadPoolExecutor(max_workers=max_workers) as executor:
        futures = {executor.submit(convert_exr_to_png_and_delete, exr_file, exr_file.replace('.exr', '.png')): exr_file for exr_file in exr_files}
        
        for future in tqdm(as_completed(futures), total=len(futures), desc="Converting EXR to PNG"):
            exr_file = futures[future]
            try:
                future.result()
            except Exception as e:
                print(f"Error processing {exr_file}: {e}")

# 示例用法
convert_all_exr_to_png_and_delete("D:\\data\\frames\\SVGF", max_workers=24)
