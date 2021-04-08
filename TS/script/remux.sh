#
../../ts -r 1920x1080-25I-IPBBB.ts
../../pes -AVC PES-0200 > /tmp/result
../../pes +R45000 PES-0200 | less

../../ts -r 1920x1080-25I-IPBBB.ts-
../../pes -AVC PES-0200 | less
../../pes PES-0201 
../../pes +R9000 PES-0201  | less

../../ts -r 1920x1080-25I-IPBBB.ts--
../../pes -AVC PES-0200 > /tmp/result
../../pes PES-0201 > /tmp/result
