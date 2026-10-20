# CMake generated Testfile for 
# Source directory: D:/PROJECT/LIINSPACE/DAL/SOFTWARE/PDU/tools/xcli_client
# Build directory: D:/PROJECT/LIINSPACE/DAL/SOFTWARE/PDU/tools/xcli_client/build-windows
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(client_help "D:/PROJECT/LIINSPACE/DAL/SOFTWARE/PDU/tools/xcli_client/build-windows/xcli_client.exe" "--help")
set_tests_properties(client_help PROPERTIES  PASS_REGULAR_EXPRESSION "Usage:" _BACKTRACE_TRIPLES "D:/PROJECT/LIINSPACE/DAL/SOFTWARE/PDU/tools/xcli_client/CMakeLists.txt;78;add_test;D:/PROJECT/LIINSPACE/DAL/SOFTWARE/PDU/tools/xcli_client/CMakeLists.txt;0;")
