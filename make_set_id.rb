#!/usr/bin/ruby

commit = `git show --pretty=%H 2>&1`
version = ENV['TRAVIS_BUILD_NUMBER']

Dir.mkdir 'exe' unless File.exists? 'exe'
File.open("./exe/build_info.txt", 'w') { |f|
  f.write "version: "+version + "\n" +
          "last_tag: "+`git describe --abbrev=0 --tags`+
          "branch: "+`git rev-parse --abbrev-ref HEAD`+
          "travis_pull_request: #{ENV['TRAVIS_PULL_REQUEST']}\n"+
          "travis_branch: #{ENV['TRAVIS_BRANCH']}\n"+
          "travis_tag: #{ENV['TRAVIS_TAG']}\n"+
          "travis_build_id: #{ENV['TRAVIS_BUILD_ID']}\n"+
          "travis_build_number: #{ENV['TRAVIS_BUILD_NUMBER']}\n"+
          "travis_commit: #{ENV['TRAVIS_COMMIT']}\n"+
          "travis_commit_range: #{ENV['TRAVIS_COMMIT_RANGE']}\n"+
          "travis_job_id: #{ENV['TRAVIS_JOB_ID']}\n"+
          "travis_job_number: #{ENV['TRAVIS_JOB_NUMBER']}\n"
}
File.open("kitsune/kitsune_version.h", 'w') { |f|
  f.write "#ifndef KIT_VER\n"+
          "#define KIT_VER "+version+"\n"+
          "#endif\n"
}

File.open("./exe/version-#{version}.txt", 'w') { |f|
  f.write "#{ENV['TRAVIS_BRANCH']}\n"
}

puts "Labelled as KIT_VER "+version+"\n"
