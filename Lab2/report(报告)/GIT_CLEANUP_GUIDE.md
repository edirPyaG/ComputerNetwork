# Git 大文件清理指南

## 当前问题
文件 `Lab2/report(报告)/tool/r77Rootkit 1.8.1.zip` (128.40 MB) 存在于 Git 历史中，导致无法推送到 GitHub。

## 解决方案

### 方法 1: 使用 BFG Repo-Cleaner（推荐）

1. 下载 BFG:
   访问 https://rtyley.github.io/bfg-repo-cleaner/
   下载 bfg.jar

2. 在 Lab2 目录执行:
   ```cmd
   java -jar bfg.jar --delete-files "r77Rootkit 1.8.1.zip"
   ```

3. 清理和推送:
   ```cmd
   git reflog expire --expire=now --all
   git gc --prune=now --aggressive
   git push origin main --force
   ```

### 方法 2: 使用 git filter-branch

在 Lab2 目录执行以下命令:

```cmd
git filter-branch --force --index-filter "git rm --cached --ignore-unmatch \"Lab2/report(报告)/tool/r77Rootkit 1.8.1.zip\"" --prune-empty --tag-name-filter cat -- --all

git for-each-ref --format="delete %(refname)" refs/original | git update-ref --stdin
git reflog expire --expire=now --all
git gc --prune=now --aggressive

git push origin main --force
```

### 方法 3: 重置到删除文件之前的提交

如果上述方法都不行，可以考虑:

1. 查看提交历史找到添加大文件之前的提交:
   ```cmd
   git log --oneline
   ```

2. 重置到那个提交（假设是 abc1234）:
   ```cmd
   git reset --hard abc1234
   git push origin main --force
   ```

## 注意事项

- 使用 `--force` 推送会重写远程历史，确保团队成员知晓
- 建议先备份仓库
- 如果有其他协作者，他们需要重新克隆仓库
