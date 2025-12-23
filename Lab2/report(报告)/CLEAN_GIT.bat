@echo off
REM ============================================
REM Git 大文件清理脚本
REM ============================================

echo.
echo ========================================
echo    清理 Git 历史中的大文件
echo ========================================
echo.

REM 切换到仓库根目录
cd /d E:\大学有关文件\计算机网络\LabGit\Lab2

echo 当前目录: %CD%
echo.
echo 正在清理文件: Lab2/report(报告)/tool/r77Rootkit 1.8.1.zip
echo.
echo 这个过程可能需要几分钟，请耐心等待...
echo.

REM 执行 filter-branch
git filter-branch --force --index-filter "git rm --cached --ignore-unmatch \"Lab2/report(报告)/tool/r77Rootkit 1.8.1.zip\"" --prune-empty --tag-name-filter cat -- --all

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [错误] filter-branch 执行失败
    echo.
    pause
    exit /b 1
)

echo.
echo [完成] filter-branch 执行成功
echo.
echo 正在清理引用...

REM 清理 refs/original
git for-each-ref --format="delete %%(refname)" refs/original | git update-ref --stdin

echo 正在清理 reflog...
git reflog expire --expire=now --all

echo 正在执行垃圾回收...
git gc --prune=now --aggressive

echo.
echo ========================================
echo    清理完成！
echo ========================================
echo.
echo 现在可以执行以下命令推送到远程:
echo.
echo    git push origin main --force
echo.
echo 注意: 这会重写远程历史，请确保团队成员知晓
echo.
pause
