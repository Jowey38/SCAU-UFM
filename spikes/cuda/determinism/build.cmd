@echo off
call "D:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat" -arch=x64 >nul
"C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.8\bin\nvcc.exe" -arch=sm_61 -O2 -o "%~dp0cuda_determinism_spike.exe" "%~dp0cuda_determinism_spike.cu"
