# macOS打包与部署验证

环境macOS26.4(build25E246)、arm64、Qt6.12.0目录式官方安装、AppleClang21、Qt自带CMake/Ninja。GUI包名IMClient.app，显示产品IMPlatform，版本0.5.0，bundle ID edu.joyce.implatform.client，构建记录的最低macOS26.4；没有验证更旧macOS或Intel Mac。

```sh
python3 scripts/generate_app_icon.py
cmake -S . -B build/mvp5 -G Ninja -DCMAKE_PREFIX_PATH="$HOME/Qt/6.12.0/macos"
cmake --build build/mvp5 --parallel 6
python3 scripts/package_macos.py --build build/mvp5 \
  --qt-root "$HOME/Qt/6.12.0/macos" --output deliverables/MVP5-macOS-arm64-final
```

脚本要求macOS、Python>=3.9、Qt macdeployqt、Apple命令行工具。输出目录已存在即拒绝覆盖，请使用新目录；不会就地修改build.app或数据库。实际macdeployqt参数-always-overwrite -verbose=2 -no-codesign仅作用复制包，随后补齐json-c、规范化框架install ID/引用，逐个otool检查所有Mach-O非系统依赖和@rpath目标。再codesign --force --deep --sign -，以及codesign --verify --deep --strict。

实际成功检查18个Mach-O，主程序arm64，包含QtCore/Gui/Widgets/Network及部署工具所需Svg/DBus等框架、Cocoa平台插件、图像插件、json-c与qt.conf；主题QSS和窗口图标由QRC嵌入，ICNS在Contents/Resources。机器独有依赖路径不得留在真实LC_LOAD_DYLIB/LC_ID_DYLIB中。dependency-audit.json记录相对文件/完整依赖。

初次检查脚本错误地把universal Mach-O的第二个架构文件标题当成依赖，产生绝对路径误报；已修正为只解析真实依赖行，原失败日志保留。不是据此宣布Qt部署库本身存在漏洞。install_name_tool会提示旧签名失效，这是最终重新ad-hoc签名前的预期诊断，最终严格验证应退出0。

独立包测试：使用部署包内的IMClient，cocoa平台启动三独立进程，实际QQ/微信单聊、QQ群/子群、真实Socket/ACK/离线及故障恢复；过程/结果见MVP5_TEST_REPORT。开发版client路径不参与这些子进程的库解析，测试driver仍来自构建目录。此验证在本机完成，没有把Qt卸载或修改系统库来测试，没有另一台干净Mac的证据。

Finder验证状态以测试报告UI25为准。默认8888若已有用户Server，不停止/覆盖它；仅验证新客户端启动和基础Socket连接，不向用户库注册/登录。完整业务用临时Server和随机端口隔离验证。Finder启动与命令行三进程启动分开列证据。

签名仅本机ad-hoc，没有Developer ID、公证、App Store发布或互联网下载Gatekeeper验证。不得宣称任意Mac可无警告安装。复制到别的Mac时仍需匹配架构/最低系统并满足其安全策略；如Gatekeeper限制，应使用符合学校/系统策略的合法签名/分发流程，不关闭系统防护。

Server作为独立终端可执行文件随交付目录附带；它仍依赖本机json-c/OpenSSL等开发环境，未声称Server为独立跨机部署包。先启动Server再打开客户端，通信仅127.0.0.1。包不内置或修改用户数据库，不捆绑个人凭据。
