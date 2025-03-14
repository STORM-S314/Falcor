#!/usr/bin/python3

# BSD 2-Clause License
# 
# Copyright (c) 2021, Christoph Neuhauser
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
# 
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import sys
import math
import skimage
import skimage.io
import skimage.metrics
import lpips
import torch
import torchvision
import warnings
from skimage.color import rgb2gray
import pyfvvdp
warnings.filterwarnings("ignore")

def skimage_to_torch(img):
    t = torchvision.transforms.Compose([torchvision.transforms.ToTensor()])
    tensor = t(skimage.img_as_float(img)).float()
    tensor = tensor[None,0:3,:,:] * 2 - 1
    return tensor

if __name__ == '__main__':
	# filename_gt = sys.argv[1]
	# filename_approx = sys.argv[2]
	filename_gt = 'D:/data/frames/GT/Mogwai.ToneMapper.dst.100.exr'
	filename_approx = 'D:/data/frames/CSVGF/Mogwai.ToneMapper.dst.100.exr'
	img_gt = skimage.io.imread(filename_gt)
	img_approx = skimage.io.imread(filename_approx)
	mse = skimage.metrics.mean_squared_error(img_gt, img_approx)
	psnr = skimage.metrics.peak_signal_noise_ratio(img_gt, img_approx)
	#data_range=img_gt.max() - img_approx.min()
	#ssim = skimage.metrics.structural_similarity(img_gt, img_approx, data_range=data_range, multichannel=True)
	
	ref_image_gray = skimage.io.imread(filename_gt, as_gray = True)
	impaired_image_gray = skimage.io.imread(filename_approx, as_gray = True)
	data_range=ref_image_gray.max() - impaired_image_gray.min()
	ssim = skimage.metrics.structural_similarity(ref_image_gray, impaired_image_gray, data_range=data_range, multichannel=True)

	img0 = skimage_to_torch(img_gt)
	img1 = skimage_to_torch(img_approx)

	fv = pyfvvdp.fvvdp(display_name='standard_4k', heatmap='threshold', device='cuda')

	Q_JOB, status = fv.predict(img_approx, img_gt, dim_order='WHC')

	print(f'MSE: {mse}')
	print(f'RMSE: {math.sqrt(mse)}')
	print(f'PSNR: {psnr}')
	print(f'SSIM: {ssim}')
	print(f"FoVVDP: {Q_JOB}")
