# G17-B1R3 权威Vehicle座位验证修复交付

日期：2026-08-23

真实Runtime已确定FAIL：`actualVehicle==expectedDragon`且`charmer==player`，只有movement transport的`GetTransSeat()==-1`。旧verifier因此错误清理实际已建立控制权的Vehicle。

B1R3使用`Vehicle::Seats`/`GetPassenger()`取得服务器权威seat；movementSeat只诊断，不决定清理；错误Vehicle/权威seat/charmer仍严格拒绝。

前像SHA=`2c7594d0f1428a767570063ac90c5f816991bf1d883fe61e45a1a28902a68199`

后像SHA=`94ff80334783e8883f0811a1a7f76595d91b729cc43684f00273abb9d955628b`

离线门：GCC14/C++20零诊断、B1R3 9项、B1旧8项、exact lifecycle、双PS AST、native stdout/stderr/exit/空格/引号、CRLF ASCII无BOM、包级、内部SHA、ZIP CRC、确定性重建和解压复验全部PASS。

唯一ZIP：

- `G17B1R3_Authoritative_Seat_Verification_Windows_20260823.zip`
- 29614字节
- 13文件
- SHA-256=`69a3cd9cda242012422f775fc44b14cd0807269b4c4182d35ad2ed02decfad70`
- 入口=`01_Install_Build_G17B1R3.cmd`
- 结果=`C:\Users\Administrator\Downloads\workspace\uploads\G17B1R3_WINDOWS_BUILD_RESULT.txt`

本包无SQL、无客户端修改、不改R5。Windows构建和Runtime仍待用户，不提前标PASS。
