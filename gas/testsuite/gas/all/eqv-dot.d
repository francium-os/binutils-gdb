#objdump: -s -j .data
#name: eqv involving dot
# bfin doesn't support 'symbol = expression'
# tic4x has 4 octets per byte
#notarget: bfin-*-* tic4x-*-*

.*: .*

Contents of section \.data:
 0000 (0+00 0+01 0+02 0+0c|000+ 010+ 020+ 0c0+) .*
 0010 (0+10 0+14 0+10 0+1c|100+ 140+ 100+ 1c0+) .*
#pass
