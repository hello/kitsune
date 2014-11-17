#!/usr/bin/ruby

commit = `git show --pretty=%H 2>&1`
version = commit.each_line().first.to_i(16) & 0xffffffff

Dir.mkdir 'exe' unless File.exists? 'exe'
File.write("./exe/build_info.txt", "version: "+version.to_s(16) + "\n")
File.write("kitsune/kitsune_version.h", 
  "#ifndef KIT_VER\n"+
  "#define KIT_VER 0x" +version.to_s(16)+"\n"+
  "#endif\n")

puts "Labelled as KIT_VER 0x"+version.to_s(16)+"\n"
