@echo off
echo.
echo ========================================
echo    终极简单方案：重建仓库
echo ========================================
echo.

cd /d E:\大学有关文件\计算机网络\LabGit\Lab2

echo 这个方法会：
echo 1. 删除所有 Git 历史
echo 2. 创建全新的初始提交
echo 3. 强制推送到远程
echo.
echo 优点：100%% 能解决问题，速度快
echo 缺点：会丢失所有历史记录
echo.
set /p confirm="确认执行吗？(输入 YES 继续): "

if /i not "%confirm%"=="YES" (
    echo 已取消
    pause
    exit /b 0
)

echo.
echo [1/4] 备份当前 .git 文件夹...
if exist .git.backup rd /s /q .git.backup
move .git .git.backup

echo [2/4] 重新初始化 Git...
git init
git add .
git commit -m "Initial commit - 清理后的版本"

echo [3/4] 添加远程仓库...
git remote add origin https://github.com/edirPyaG/ComputerNetwork.git

echo [4/4] 强制推送...
git push -u origin main --force

echo.
echo ========================================
echo    完成！
echo ========================================
echo.
echo 如果出现问题，可以恢复备份:
echo    rd /s /q .git
echo    move .git.backup .git
echo.
pause
