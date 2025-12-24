@echo off
chcp 65001 >nul
echo.
echo ========================================
echo    BFG 快速清理 Git 大文件
echo ========================================
echo.

cd /d E:\大学有关文件\计算机网络\LabGit\Lab2

echo [1/3] 下载 BFG...
curl -L -o bfg.jar https://repo1.maven.org/maven2/com/madgag/bfg/1.14.0/bfg-1.14.0.jar

if not exist bfg.jar (
    echo.
    echo [错误] BFG 下载失败，请手动下载:
    echo https://repo1.maven.org/maven2/com/madgag/bfg/1.14.0/bfg-1.14.0.jar
    echo.
    echo 下载后放到当前目录，然后执行:
    echo java -jar bfg.jar --delete-files "r77Rootkit 1.8.1.zip"
    echo git reflog expire --expire=now --all
    echo git gc --prune=now --aggressive
    echo git push origin main --force
    pause
    exit /b 1
)

echo.
echo [2/3] 使用 BFG 清理大文件...
java -jar bfg.jar --delete-files "r77Rootkit 1.8.1.zip"

echo.
echo [3/3] 清理和推送...
git reflog expire --expire=now --all
git gc --prune=now --aggressive

echo.
echo ========================================
echo    清理完成！
echo ========================================
echo.
echo 现在执行推送:
echo    git push origin main --force
echo.
pause

git push origin main --force
