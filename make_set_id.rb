#!/usr/bin/ruby

commit = `git show --pretty=%H 2>&1`
version = commit.each_line().first.to_i(16) & 0xffffffff

Dir.mkdir 'exe' unless File.exists? 'exe'
File.open("./exe/build_info.txt", 'w') { |f|
  f.write "version: "+version.to_s(16) + "\n" +
          "tag: "+`git describe --tags`+
          "branch: "+`git rev-parse --abbrev-ref HEAD`
}
File.open("kitsune/kitsune_version.h", 'w') { |f|
  f.write "#ifndef KIT_VER\n"+
          "#define KIT_VER 0x" +version.to_s(16)+"\n"+
          "#endif\n"
}

puts "Labelled as KIT_VER 0x"+version.to_s(16)+"\n"
