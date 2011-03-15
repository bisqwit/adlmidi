<?php

/* PLAN: Encode in 720x2800.
 *         Horizontally, do 640->720 by inserting blank 8th columns.
 *         Vertically, 400*7 = 2800 by pixel scaling
 *                     350*2 = 700 by the algorithm below (blur+scanlines)
 *                     700*4 = 2800.
 * SCRATCH THAT.
 * Use machine=ega, so we can do encode to 720x350 and blur in postprocess.
 * After adding scanlines+blur, we get 720x700.
 */
$im = ImageCreateFromPng('egaham_inputsample.png');

$sx = ImageSx($im);
$sy = ImageSy($im);
$sx2 = $sx*2*9/8;
$sy2 = $sy*2;
$im2 = ImageCreateTrueColor($sx2, $sy2);

$pow = Array();
for($y=0; $y<$sy; ++$y)
  for($x=0; $x<$sx; ++$x)
  {
    $p = ImageColorAt($im, $x,$y);
    $pr = $p>>16;
    $pg = ($p>>8)&0xFF;
    $pb = $p&0xFF;
    $pow[$y][$x][0] = pow($pr/255,4);
    $pow[$y][$x][1] = pow($pg/255,3);
    $pow[$y][$x][2] = pow($pb/255,6);
  }

for($y=0; $y<$sy; ++$y)
{
  $r=0;
  $g=0;
  $b=0;
  $done=0;
  for($x=0; $x<$sx2; ++$x)
  {
    $xx = ($x/2);
    $col = $xx%9;
    $xx = ($xx-$col)*8/9 + $col;
    if($col==8)
      $color=0;
    else
      $color = @ImageColorAt($im,$xx,$y);
    if($color == 0x808080)     $color = 0x005F87;
    elseif($color == 0x000080) $color = 0x00005F;

    $rr = $color>>16;
    $gg = ($color>>8)&0xFF;
    $bb = $color&0xFF;

    #if($y < 50 || $y > 320)
    for($yd=-4; $yd<=4; $yd+=1)
    {
      if($y+$yd<0||$y+$yd>=$sy) continue;
      $po=&$pow[$y+$yd];
      for($xd=-4; $xd<=4; $xd+=1)
      {
        if($xx+$xd<0||$xx+$xd>=$sx)continue;
        $power = 36/(1+pow($xd*$xd + $yd*$yd, 1.9));
        $pox = &$po[$xx+$xd];
        $rr += $pox[0]*$power;
        $gg += $pox[1]*$power;
        $bb += $pox[2]*$power;
      }
    }

    $done = min(0.9, max($done-1, 0));
    $rounds=0;
    $factor = 100;
    for(; $done<1 && ($r!=$rr||$g!=$gg||$b!=$bb); )
    {
      $gdiff = (int)abs($g-$gg);
      $rdiff = (int)abs($r-$rr);
      $bdiff = (int)abs($b-$bb);
      $gcase=0;
      $rcase=1;
      $bcase=2;
      if($rdiff > $gdiff)
        { $gcase=1;$rcase=0; }
      if($bdiff > $gdiff && $bdiff > $rdiff)
        { ++$gcase; ++$rcase; $bcase=0; }
      for($case=0; $case<3 && $done<1; ++$case)
      { if($case == $gcase && $g != $gg) { $g = $gg; $done+=$gdiff/$factor; }
        if($case == $rcase && $r != $rr) { $r = $rr; $done+=$rdiff/$factor; }
        if($case == $bcase && $b != $bb) { $b = $bb; $done+=$bdiff/$factor; } }
      if(++$rounds >= 3) break;
    }
    
    $r = pow($r/255, 0.7)*255;
    $g = pow($g/255, 0.7)*255;
    $b = pow($b/255, 0.7)*255;

    MakeRgb($r2,$g2,$b2, $r/2,$g/2,$b/2);
    MakeRgb($r,$g,$b, $r,$g,$b);
    ImageSetPixel($im2, $x, $y*2+0, ImageColorAllocate($im2,$r,$g,$b));
    ImageSetPixel($im2, $x, $y*2+1, ImageColorAllocate($im2,$r2,$g2,$b2));
  }
}

ImagePng($im2, 'egaham_test.png');

function MakeRgb(&$dr, &$dg, &$db, $r,$g,$b)
{
  $dr=$r;
  $dg=$g;
  $db=$b;
  if($dr>255) { $dg+=($dr-255)/2; $db+=($dr-255)/2; $dr=255; }
  if($dg>255) { $dr+=($dg-255)/2; $db+=($dg-255)/2; $dg=255; }
  if($db>255) { $dr+=($db-255)/2; $dg+=($db-255)/2; $db=255; }
  if($dr>255) $dr=255;
  if($dg>255) $dg=255;
  if($db>255) $db=255;
}
