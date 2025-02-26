import os
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor, as_completed
from tqdm import tqdm
from moviepy.video.io.ImageSequenceClip import ImageSequenceClip

# 定义源文件夹和目标视频文件夹路径
source_folder = 'D:\\data\\frames\\SVGF'
target_folder = 'D:\\UnityProject\\CSVGF\\Assets\\Resources\\Data\\videos\\SVGF'

# 创建一个字典来存储不同类型文件的列表
files_by_type = defaultdict(list)

# 遍历源文件夹中的文件
for filename in os.listdir(source_folder):
    if filename.endswith('.png'):
        parts = filename.split('.')
        if len(parts) == 5:
            file_type = parts[2]
            file_idx = int(parts[3])
            files_by_type[file_type].append((file_idx, filename))

# 定义函数来将一组图片转换为视频
def convert_images_to_video(file_type, files):
    files.sort(key=lambda x: x[0])  # 按文件名排序
    image_files = [os.path.join(source_folder, file[1]) for file in files]
    video_path = os.path.join(target_folder, f'{file_type}.mp4')
    
    # 创建视频剪辑
    clip = ImageSequenceClip(image_files, fps=24)
    
    # 保存视频
    clip.write_videofile(video_path, codec='libx264',logger=None,threads=6)

# 使用线程池和tqdm进度条来转换图片为视频
with ThreadPoolExecutor(max_workers=4) as executor:
    futures = {executor.submit(convert_images_to_video, file_type, files): file_type for file_type, files in files_by_type.items()}
    
    for future in tqdm(as_completed(futures), total=len(futures), desc="Converting images to videos"):
        file_type = futures[future]
        try:
            future.result()
        except Exception as e:
            print(f"Error processing {file_type}: {e}")

print("视频生成完成")
