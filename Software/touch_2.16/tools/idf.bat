# ESP-IDF 环境引导脚本（Windows / cmd）
# 用法： tools\idf.bat build   /   tools\idf.bat -p COM7 flash monitor
@echo off
setlocal
set "IDF_PATH=D:\Espressif\Espressif\frameworks\esp-idf-v5.5.2"
set "IDF_TOOLS_PATH=D:\Espressif\Espressif"
set "IDF_PYTHON_ENV_PATH=D:\Espressif\Espressif\python_env\idf5.5_py3.11_env"
call "%IDF_PATH%\export.bat" >nul
cd /d "%~dp0.."
idf.py %*
endlocal