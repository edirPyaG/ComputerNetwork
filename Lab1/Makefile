CC=cl.exe
CFLAGS=/EHsc /std:c++17 /utf-8 /I include /I lib\sqlitex64 /W4
LDFLAGS=ws2_32.lib
OBJDIR=build

.PHONY: all clean

all: $(OBJDIR) $(OBJDIR)\Server.exe $(OBJDIR)\Client.exe

$(OBJDIR):
    if not exist "$(OBJDIR)" mkdir "$(OBJDIR)"

# 编译每个源文件到 build 下的 .obj（Common 只编译一次）
$(OBJDIR)\Server.obj: src\Server.cpp
    $(CC) $(CFLAGS) /c src\Server.cpp /Fo$(OBJDIR)\Server.obj

$(OBJDIR)\Client.obj: src\Client.cpp
    $(CC) $(CFLAGS) /c src\Client.cpp   /Fo$(OBJDIR)\Client.obj

$(OBJDIR)\Common.obj: src\Common.cpp
    $(CC) $(CFLAGS) /c src\Common.cpp /Fo$(OBJDIR)\Common.obj

$(OBJDIR)\Storage.obj: src\Storage.cpp
    $(CC) $(CFLAGS) /c src\Storage.cpp /Fo$(OBJDIR)\Storage.obj

# 统一把 SQLite 源文件编译为一个对象文件（只编译一次）
$(OBJDIR)\sqlite3.obj: lib\sqlitex64\sqlite3.c
	$(CC) $(CFLAGS) /c lib\sqlitex64\sqlite3.c /Fo$(OBJDIR)\sqlite3.obj

# 链接生成可执行文件到 build
$(OBJDIR)\Server.exe: $(OBJDIR)\Server.obj $(OBJDIR)\Common.obj $(OBJDIR)\sqlite3.obj
	$(CC) $(OBJDIR)\Server.obj $(OBJDIR)\Common.obj $(OBJDIR)\sqlite3.obj /link /OUT:$@ $(LDFLAGS)

$(OBJDIR)\Client.exe: $(OBJDIR)\Client.obj $(OBJDIR)\Common.obj $(OBJDIR)\Storage.obj $(OBJDIR)\sqlite3.obj
	$(CC) $(OBJDIR)\Client.obj $(OBJDIR)\Common.obj $(OBJDIR)\Storage.obj $(OBJDIR)\sqlite3.obj /link /OUT:$@ $(LDFLAGS)

clean:
    if exist "$(OBJDIR)\*.exe" del /Q "$(OBJDIR)\*.exe"
    if exist "$(OBJDIR)\*.obj" del /Q "$(OBJDIR)\*.obj"





