#!/bin/bash
ffmpeg -i $1 -f s16le -acodec pcm_s16le $2