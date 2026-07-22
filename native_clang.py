Import("env")
env.Replace(CC="clang", CXX="clang++", LINK="clang++")
env.Append(LINKFLAGS=["-fsanitize=address,undefined"])
