import os
import shutil
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor
from tqdm import tqdm

# 定义源文件夹和目标文件夹路径
source_folder = 'D:\\data\\frames\\ASVGF'
target_folder = 'D:\\UnityProject\\CSVGF\\Assets\\Resources\\Data\\frames\\ASVGF'

# 创建一个字典来存储不同类型文件的列表
files_by_type = defaultdict(list)

# 遍历源文件夹中的文件
for filename in os.listdir(source_folder):
    if filename.endswith('.png'):
        parts = filename.split('.')
        if len(parts) == 5:
            file_type = parts[2]
            file_idx = int(parts[3])
            files_by_type[file_type].append((file_idx,filename))

# 准备要复制的文件列表
files_to_copy = []
for file_type, files in files_by_type.items():
    files.sort(key=lambda x: x[0])  # 按文件名排序
    files_to_copy.extend([file[1] for file in files[1300:1400]])

# 定义复制函数
def copy_file(file):
    shutil.copy(os.path.join(source_folder, file), target_folder)

# 使用线程池和tqdm进度条来复制文件
with ThreadPoolExecutor(max_workers=24) as executor:
    list(tqdm(executor.map(copy_file, files_to_copy), total=len(files_to_copy)))

print("复制完成")

