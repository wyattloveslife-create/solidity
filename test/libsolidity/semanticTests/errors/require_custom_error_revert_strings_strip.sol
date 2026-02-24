contract C {
    error MyError(uint errorCode, string errorMsg, bool flag);
    function flag() pure public returns (bool) { return false; }
    function f() pure external {
        uint code = 8;
        string memory eMsg = "error";
        require(false, MyError(code, eMsg, flag()));
    }
}
// ====
// revertStrings: strip
// ----
// f() -> FAILURE, hex"2ced9160", hex"0000000000000000000000000000000000000000000000000000000000000008", hex"0000000000000000000000000000000000000000000000000000000000000060", hex"0000000000000000000000000000000000000000000000000000000000000000", hex"0000000000000000000000000000000000000000000000000000000000000005", hex"6572726f72000000000000000000000000000000000000000000000000000000"
