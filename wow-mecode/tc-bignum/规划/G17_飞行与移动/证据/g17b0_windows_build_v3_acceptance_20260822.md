# G17-B0 Windows构建v3真实结果验收

日期：2026-08-22（Asia/Shanghai）  
用户文件：`G17B0_WINDOWS_BUILD_RESULT.txt`  
证据副本：`G17B0_WINDOWS_BUILD_V3_RESULT_20260822.txt`

## 不可变身份

- 字节数：6396
- SHA-256：`58c408728f6a8a91a317abd26e10911a346b607e7adaf0cbb3880ec656a6952e`
- 逻辑行：122
- 验收器：`accept_g17b0_windows_build_v3.py`

## 正式通过项

```text
G17B0_BUILD_SOURCE_GATE=PASS
G17B0_NATIVE_RUNNER_SELFTEST=PASS
CMAKE_EXIT=0
DRAGONRIDING_VCXPROJ_HITS=1
G17B0_CMAKE_SOURCE_MEMBERSHIP=PASS
MSBUILD_EXIT=0
DRAGONRIDING_FRESH_OBJECTS=1
G17B0_WINDOWS_BUILD_PASS=True
G17B0_WINDOWS_BUILD_RESULT=PASS
G17B0_WINDOWS_BUILD_COMPLETE
```

CMake完成Configure/Generate并写入`D:/TC-Build`。`scripts.vcxproj`包含新文件；MSBuild明确编译`cs_dragonriding.cpp`和`cs_script_loader.cpp`、生成`scripts.lib`并链接`worldserver.exe`。

构建前后：

```text
before exe size=36739072
before exe sha256=a43fcf10567db83ef8b96ed9b47567860fd786dd40f6aeb0841e6d643583cb80
after exe size=36759040
after exe sha256=59491e97426dc059e2f440b6ca17f28ffdc6eb296e3f8d04fc428b59099b5881
before pdb size=433344512
after pdb size=433442816
fresh object=D:\TC-Build\src\server\scripts\scripts.dir\RelWithDebInfo\cs_dragonriding.obj
fresh object size=807675
```

EXE/PDB时间均前进，EXE哈希变化。

## 唯一警告

```text
cs_dragonriding.cpp(119,40): warning C4018
```

位置是`GetPower(POWER_ENERGY) < cost`的有符号/无符号比较。三个调用者只传入正数常量20/30/25，因此本次为非致命类型告警，不影响本轮链接PASS；已登记为后续源码清理项，不能扩写为零警告构建。

## 结论与边界

```text
G17B0_WINDOWS_BUILD_V3_ACCEPTANCE=PASS
G17B0_WINDOWS_BUILD=PASS
G17B0_RUNTIME=NOT_RUN
```

本结果证明新源码进入工程、编译并链接到新worldserver；不证明服务器已成功加载world数据，也不证明游戏内座位、动作条、四技能、能量、禁区和清理路径已通过。下一阶段允许进入受控启动与Runtime验收。
