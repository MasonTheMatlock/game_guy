//#############################################################################
// FILE:   app/wason/wason_art.c
// TITLE:  Wason Ware pixel-art sprite tables (data only)
//
//   Each sprite is an array of strings, one per pixel row. ww_DrawArt() /
//   ww_MoveArt() (wason_helpers.c) turn the letters into colors.
//#############################################################################
#include "wason_priv.h"

//*****************************************************************************
// Pixel art (generated). One string per row; '.' = background. See DrawArt().
//*****************************************************************************

const char *const ww_kArtFire[16] =
{
    ".......kk.......",
    ".....kkookk.....",
    "....kooooook....",
    "...kooooooook...",
    "...kooooooook...",
    "..koowkoookwokk.",
    "...kokoooookkrrk",
    "...kooooooookrrk",
    "....kookkookraar",
    "....kooyyookkaak",
    "...kooyyyyooorrk",
    "...koyyyyyyooook",
    "...koyyyyyyoook.",
    "....koyyyyoook..",
    "....kooyyookk...",
    "....kookkook...."
};

const char *const ww_kArtWater[16] =
{
    "................",
    ".....kkkkkk.....",
    "....kbbbbbbk....",
    "...kbbbbbbbbk...",
    "...kbbbbbbbbk...",
    "...kbwkbbbkwk...",
    "...kbkbbbbbk.kk.",
    "...kbbbbbbbbkbck",
    "....kbbkkbbkbbbk",
    "...knccccccnbbbk",
    "..knnyyyyyynbbbk",
    "..knnyyyyyynnbk.",
    "..knnyyyyyynnk..",
    "..knbbyyyybbnk..",
    "...kbbnnnnbbk...",
    "...kbbknnkbbk..."
};

const char *const ww_kArtGrass[16] =
{
    ".....kkdgkk.....",
    "....kdllgldk....",
    "...kdlllllldk...",
    "...kdlllllldk...",
    "...kdlllllldk...",
    "...kddllllddk...",
    "..kttddddddttk..",
    ".kttttttttdtttk.",
    ".kttwtttttddwtk.",
    "ktttrrtttttrrtk.",
    "ktttdtttttttttk.",
    ".kkttttttttttk..",
    "..kttkttttkttk..",
    "..kttkkkkkkttk..",
    "..kttk....kttk..",
    "...kk......kk..."
};

const char *const ww_kArtSonic0[20] =
{
    "kk..............",
    "bbkkk..kkkk.....",
    "bbbbbkkbbbbk....",
    "kbbbbbbbbwwwk...",
    ".kkkbbbbbwwkbk..",
    "kkbbbbbbbwwkkk..",
    "bbbbbbbbbwwwppk.",
    "bbbbkbbbbbppppk.",
    "kkkbbbbbbbbpppk.",
    ".kbbbbkbbbbkkk..",
    "kbbbbkbbbbbk....",
    "bbbbkkbbbppk....",
    "bbkkkbbbppppk...",
    "kk..kbbbpppppk..",
    ".....kbbppppppk.",
    "....kbbbbbbkppk.",
    "....kbbkbbbbkk..",
    "...kbbbkkkbbbkkk",
    "..krwwrrk.krwwrr",
    "..krrrrrk.krrrrr"
};

const char *const ww_kArtSonic1[20] =
{
    "kk..............",
    "bbkkk..kkkk.....",
    "bbbbbkkbbbbk....",
    "kbbbbbbbbwwwk...",
    ".kkkbbbbbwwkbk..",
    "kkbbbbbbbwwkkk..",
    "bbbbbbbbbwwwppk.",
    "bbbbkbbbbbppppk.",
    "kkkbbbbbbbbpppk.",
    ".kbbbbkbbbbkkk..",
    "kbbbbkbbbbbk....",
    "bbbbkkbbbppk....",
    "bbkkkbbbppppk...",
    "kk..kbbbpppppk..",
    ".....kbbppppppk.",
    ".....kbbbbbkppk.",
    "....kkkbbbbkkk..",
    "...krrrrbbkk....",
    "...krrrrwwrrk...",
    "....kkkrrrrrk..."
};

const char *const ww_kArtSonic2[20] =
{
    "kk..............",
    "bbkkk..kkkk.....",
    "bbbbbkkbbbbk....",
    "kbbbbbbbbwwwk...",
    ".kkkbbbbbwwkbk..",
    "kkbbbbbbbwwkkk..",
    "bbbbbbbbbwwwppk.",
    "bbbbkbbbbbppppk.",
    "kkkbbbbbbbbpppk.",
    ".kbbbbkbbbbkkk..",
    "kbbbbkbbbbbk....",
    "bbbbkbbbpppbk...",
    "bbkkkbbbpppbk...",
    "kk..kbbbpppbk...",
    "...kkkrrrrpkk...",
    "..krrrrrrrrrrk..",
    ".krrwwwwwwwwrrk.",
    ".krrwwwwwwwwrrk.",
    "..krrrrrrrrrrk..",
    "...kkkrrrrkkk..."
};

const char *const ww_kArtSpin[16] =
{
    ".....kkkkkk.....",
    "....kbbbbbbk....",
    "...kbbbbbbbbk...",
    "..kbbbbbbbbbbk..",
    ".kbbbccccccbbbk.",
    "kbbbccccccccbbbk",
    "kbbbccccccccbbbk",
    "kwwwwwwwwwwwwwwk",
    "kwwwwwwwwwwwwwwk",
    "kbbbccccccccbbbk",
    "kbbbccccccccbbbk",
    ".kbbbccccccbbbk.",
    "..kbbbbbbbbbbk..",
    "...kbbbbbbbbk...",
    "....kbbbbbbk....",
    ".....kkkkkk....."
};

const char *const ww_kArtMotobug[12] =
{
    "..............",
    "....kkkkkk....",
    "...krrrrrrk...",
    "..krrrrrrkrk..",
    ".krrkrrrrrrrk.",
    "krrrrrrrrrrrrk",
    "krrrrrrkrrrrrk",
    "rrrrrrrrrwwrrr",
    "rrrrrrrrrwwkrr",
    "reeerrrrrrrrrr",
    "keeerrrrrreeek",
    ".kkkrrrrrreeek"
};

const char *const ww_kArtRing[10] =
{
    "..hhhhhh..",
    ".hyyyyyyh.",
    "hyyyyyyyyh",
    "hyyhhhhyyh",
    "hyyh..hyyh",
    "hyyh..hyyh",
    "hyyhhhhyyh",
    "hyyyyyyyyh",
    ".hyyyyyyh.",
    "..hhhhhh.."
};

const char *const ww_kArtSpikes[10] =
{
    "kssskkssskksssk.",
    "kssskkssskksssk.",
    "kssk.kssk.kssk..",
    "kssk.kssk.kssk..",
    "kssk.kssk.kssk..",
    "kssk.kssk.kssk..",
    "ssskkssskksssk..",
    "sskkksskkksskkkk",
    "ssssssssssssssss",
    "ssssssssssssssss"
};

const char *const ww_kArtSpring[10] =
{
    "..kkkkkkkkkk..",
    ".kyyyyyyyyyyk.",
    ".kyyyyyyyyyyk.",
    "..kssssssssk..",
    "...kkkkkkkk...",
    ".kksssssssskk.",
    "krrrrrrrrrrrrk",
    "krrrrrrrrrrrrk",
    "krrrrrrrrrrrrk",
    "krrrrrrrrrrrrk"
};

const char *const ww_kArtSunflower[20] =
{
    ".kyyyyyyyyk.",
    ".kyyyyyyyyk.",
    ".kyynnnnyyk.",
    "kyyynnnnyyyk",
    "kyyynnnnyyyk",
    ".kyynnnnyyk.",
    ".kyyyyyyyyk.",
    ".kyyyyyyyyk.",
    "..kkkyykkk..",
    "....kddk....",
    "kkkkkddk....",
    "dddddddk....",
    "dddddddkkkkk",
    "kkkkkddddddd",
    "....kddddddd",
    "....kddkkkkk",
    "....kddk....",
    "....kddk....",
    "....kddk....",
    "....kddk...."
};

const char *const ww_kArtPalm[30] =
{
    "....................",
    ".........kkk........",
    "........kgggk.......",
    "...kkk..kgggk.kkkk..",
    "..kgggk.kgggkkggggk.",
    "..kggggkkgggkgggggk.",
    "..kgggggggggggggggk.",
    "...kggggggggggggkk..",
    "kkkkkgggggggggggk...",
    "gggggggggnnnnggkkkkk",
    "gggggggggnnnnggggggg",
    "gggggggggnnnnggggggg",
    "kkkkkkkkknnnnggggggg",
    ".........knnnnkkkkkk",
    ".........knnnk......",
    ".........knnnk......",
    ".........knnnk......",
    ".........knnnk......",
    ".........knnnk......",
    "........knnnnk......",
    "........knnnnk......",
    "........knnnnk......",
    "........knnnk.......",
    "........knnnk.......",
    "........knnnk.......",
    "........knnnk.......",
    "........knnnk.......",
    "........knnnk.......",
    "........knnnk.......",
    "........knnnk......."
};

const char *const ww_kArtOrange[16] =
{
    ".......kkkkkk...",
    "......knnggggk..",
    ".....kknggggggk.",
    "...kkooooggggk..",
    "..kooooooooook..",
    "..kooooooooook..",
    ".kooaaaaooooook.",
    ".kooaaaaooooook.",
    ".kooooooooooook.",
    ".kooooooooooook.",
    ".kooooooooooook.",
    ".kooooooooooook.",
    "..kooooooooook..",
    "..kooooooooook..",
    "...kkooooookk...",
    ".....kkkkkk....."
};

const char *const ww_kArtApple[16] =
{
    "........kkkkk...",
    ".......knggggk..",
    "....kkkknggggk..",
    "...krrrknrrrk...",
    "..krrrrrrrrrrk..",
    ".krrrrrrrrrrrrk.",
    "krrrrwrrrrrrrrrk",
    "krrrrwrrrrrrrrrk",
    "krrrrrrrrrrrrrrk",
    "krrrrrrrrrrrrrrk",
    "krrrrrrrrrrrrrrk",
    ".krrrrrrrrrrrrk.",
    "..krrrrrrrrrrk..",
    "...krrrrrrrrk...",
    "....kkrrrrkk....",
    "......kkkk......"
};

const char *const ww_kArtLime[16] =
{
    ".....kkkkkk.....",
    "....kggggggk....",
    "...kgglgllggk...",
    "..kglllgllllgk..",
    ".kglgllglllglgk.",
    "kggllgaagagllggk",
    "kglllagaggalllgk",
    "kglllaaggaalllgk",
    "kggggggggggggggk",
    "kglllagaggalllgk",
    "kggllgaagagllggk",
    ".kglglllgllglgk.",
    "..kgllllglllgk..",
    "...kggllglggk...",
    "....kggggggk....",
    ".....kkkkkk....."
};

const char *const ww_kArtBomb[16] =
{
    ".......kkooorak.",
    "......knnokkkk..",
    ".....kknnkk.....",
    "...kkeenneekk...",
    "..keeeeeeeeeek..",
    "..keeeeeeeeeek..",
    ".keesssseeeeeek.",
    ".keesssseeeeeek.",
    ".keeeeeeeeeeeek.",
    ".keeeeeeeeeeeek.",
    ".keeeeeeeeeeeek.",
    ".keeeeeeeeeeeek.",
    "..keeeeeeeeeek..",
    "..keeeeeeeeeek..",
    "...kkeeeeeekk...",
    ".....kkkkkk....."
};

const char *const ww_kArtPeashooter[16] =
{
    ".....kkk........",
    "...kkgllk.......",
    "..kggllllk......",
    ".kggggllggkkkk..",
    ".kggggggggggggk.",
    "kgggwkgggggggddk",
    "kgggkggggggggdk.",
    ".kgggggggggggddk",
    ".kggggggggggggk.",
    "..kggggggkkkkk..",
    "kkkkkddkk.......",
    "dddddddkkkkkk...",
    "dddddddddddddk..",
    "kkkkkddddddddk..",
    "....kddkkkkkk...",
    ".....kk........."
};

const char *const ww_kArtZombie[22] =
{
    ".knnnkkkk...",
    ".knnnzzzzk..",
    "..kzzzzzzk..",
    ".kzzwwzzzzk.",
    ".kzkzzkzzzk.",
    ".kzzzzzzzzk.",
    "..kzzzzzzk..",
    "...kkkkzzk..",
    "...kkkkkkk..",
    "kkknnrrnnnk.",
    "zzzznrrnnnk.",
    "zzzznrrnnnk.",
    "zzknnrrnnnk.",
    "kkknnrrnnnk.",
    "..knnnnnnnk.",
    "..knnnnnnnk.",
    "...keeekeeek",
    "...keeekeeek",
    "...keeekeeek",
    "...keeekeeek",
    "...kkkk.kkkk",
    "...kkkk.kkkk"
};

const char *const ww_kArtPea[6] =
{
    ".dlld.",
    "dlllld",
    "llwlll",
    "llllll",
    "dlllld",
    ".dlld."
};

const char *const ww_kArtRun0[22] =
{
    "..kbbbbbbbbk..",
    "..kbbbbbbbbk..",
    ".kbbbbbbbbbbk.",
    "kbbbbnnnnnnk..",
    ".kknnnnnnnnk..",
    "...kuuuuuuk...",
    "...knnnnnnk...",
    "...kkknnkkk...",
    "..kyyyyyyyyk..",
    ".kooooooooook.",
    "kppooooooooppk",
    "kppooooooooppk",
    "kppooooooooppk",
    "kppooooooooppk",
    "kppooooooooppk",
    ".kkooooooookk.",
    "..kuuukkuuuk..",
    "..kuuukkuuuk..",
    "..kuuukkuuuk..",
    "..kuuukkwwwk..",
    "..kuuuk.kkk...",
    ".kwwwwk......."
};

const char *const ww_kArtRun1[22] =
{
    "..kbbbbbbbbk..",
    "..kbbbbbbbbk..",
    ".kbbbbbbbbbbk.",
    "kbbbbnnnnnnk..",
    ".kknnnnnnnnk..",
    "...kuuuuuuk...",
    "...knnnnnnk...",
    "...kkknnkkk...",
    "..kyyyyyyyyk..",
    ".kooooooooook.",
    "kppooooooooppk",
    "kppooooooooppk",
    "kppooooooooppk",
    "kppooooooooppk",
    "kppooooooooppk",
    ".kkooooooookk.",
    "..kuuukkuuuk..",
    "..kuuukkuuuk..",
    "..kuuukkuuuk..",
    "..kwwwkkuuuk..",
    "...kkk.kuuuk..",
    ".......kwwwwk."
};

const char *const ww_kArtBall[12] =
{
    "...kkkkkk...",
    "..krrrrrrk..",
    ".krrrrrrrrk.",
    "krrrrrrrrrrk",
    "krrrrwwrrrrk",
    ".kkkwwwwkkk.",
    ".kkkwwkwkkk.",
    "kwwwwwwwwwwk",
    "kwwwwwwwwwwk",
    ".kwwwwwwwwk.",
    "..kwwwwwwk..",
    "...kkkkkk..."
};

const char *const ww_kArtBallHit[12] =
{
    "...kkkkkk...",
    "..kaaaaaak..",
    ".kaaaaaaaak.",
    "kaaaaaaaaaak",
    "kaaaawwaaaak",
    ".kkkwwwwkkk.",
    ".kkkwwkwkkk.",
    "kwwwwwwwwwwk",
    "kwwwwwwwwwwk",
    ".kwwwwwwwwk.",
    "..kwwwwwwk..",
    "...kkkkkk..."
};

const char *const ww_kArtPikachu[16] =
{
    ".kkyk.kkkk.kyykk",
    ".kyykkyyyykkyyk.",
    ".kyyyyyyyyyyyyk.",
    "..kyyyyyyyyyyk..",
    "...kyywyyywyk...",
    "...kyykyyykyk...",
    "..krryyyyyyrrkkk",
    "..krryyykyyrrkyy",
    "..kyyyyyyyyyyyyy",
    "..kyyyyyyyyyyyyk",
    "..kyyyyyyyyyyyyk",
    "..kyyyyyyyyyyyk.",
    "..kyyyyyyyyyyk..",
    "...kyyyyyyyyk...",
    "....kyyyyyyk....",
    "....kyykkyyk...."
};

const char *const ww_kArtDiglett[12] =
{
    "..............",
    "......kk......",
    "...kkknnkkk...",
    "..knnnnnnnnk..",
    ".knnnnnnnnnnk.",
    "knnnnnnnnnnnnk",
    "knnnknnnnnknnk",
    "knnnnppppnnnnk",
    "knnnnppkpnnnnk",
    "knnnnppppnnnnk",
    "nnnnnnnnnnnnnn",
    "nnnnnnnnnnnnnn"
};

const char *const ww_kArtGdSpike[12] =
{
    "......w......",
    ".....wrw.....",
    "....wrrrw....",
    "....wrrrw....",
    "...wrrrrrw...",
    "...wrrrrrw...",
    "..wrrrrrrrw..",
    "..wrrrrrrrw..",
    ".wrrrrrrrrrw.",
    ".wrrrrrrrrrw.",
    "wrrrrrrrrrrrw",
    ".wwwwwwwwwww."
};

const char *const ww_kArtNote[12] =
{
    "...kkkkkk...",
    "..kmmmmmmk..",
    ".kmmmmmmmmk.",
    "kmmmmwwmmmmk",
    "kmmmwwwwmmmk",
    "kmmwwwwwwmmk",
    "kmmwwwwwwmmk",
    "kmmmwwwwmmmk",
    "kmmmmwwmmmmk",
    ".kmmmmmmmmk.",
    "..kmmmmmmk..",
    "...kkkkkk..."
};

const char *const ww_kArtMetroid[14] =
{
    "...kkllllllkk...",
    "..kllllggllllk..",
    ".kllggggggggllk.",
    ".klgwgrrrrggwlk.",
    "kllgggrrrrgggllk",
    "klggggrrrrgggglk",
    "klggggrrrrgggglk",
    "kllggggggggggllk",
    "kllggllgllggllk.",
    "klllgllgllggllk.",
    "kllllllgllllllk.",
    "kllkkllllllkllk.",
    "klllklllllkkllk.",
    "klllklllllkkllk."
};

const char *const ww_kArtSamus[22] =
{
    "...kkkkkk.....",
    "..kooooook....",
    "..kooooook....",
    ".kooooggggk...",
    ".kooooggggk...",
    ".kooooggggk...",
    "..kooooook....",
    "..kooooook....",
    "..kkkkkkkkk...",
    ".koooooooook..",
    ".koooooorrrrk.",
    ".koooooorrrrk.",
    ".kyyyooorrrrk.",
    ".kyyyooorrrrk.",
    ".kyyyooooook..",
    ".koooooooook..",
    ".koooooooook..",
    "..koookoook...",
    "..koookoook...",
    "..koookoook...",
    "..koookoook...",
    ".kyyyykyyyyk.."
};

const char *const ww_kArtReo[10] =
{
    "uukk....kkuu",
    "uuuuk..kuuuu",
    "kuuuukkuuuuk",
    ".kuuuuuuuuk.",
    "..kkururkk..",
    "...kmmmmk...",
    "...kmmmmk...",
    "....kkkk....",
    "............",
    "............"
};

const char *const ww_kArtWario[24] =
{
    "........kkkkkkkk........",
    "......kkvvvvvvvvkk......",
    "....kkvvvvvvvvvvvvkk....",
    "...kvvvvvvaaaavvvvvvk...",
    "...kvvvvvaaaaaavvvvvk...",
    "..kvvvvvvaaxaxavvvvvvk..",
    "..kvvvvvvaaaxaavvvvvvk..",
    "...kvvvvvppppppvvvvvk...",
    "..kxkkkkppppppppkkkkk...",
    "..kxkkkkkkppppkkkkkkkk..",
    ".kppppkkkkkkkkkkkkppppk.",
    ".kppppwwwkkkkkkkwwppppk.",
    "kpppppwwwkppppwkwwpppppk",
    ".kppppppppffffppppppppk.",
    ".kpppppppffffffpppppppk.",
    "..kppppppffffffppppppk..",
    "...kppkkkkkffkkkkkppk...",
    "...kkkkkkkkkkkkkkkkkk...",
    "..kvkkkkkkkkkkkkkkkkvk..",
    ".kvvvvkppppppppppkvvvvk.",
    "kvvvvvvppppppppppvvvvvvk",
    "kvvvvvvppppppppppvvvvvvk",
    "vvvvvvvppppppppppvvvvvvv",
    "kvvvvvvppppppppppvvvvvvk"
};

const char *const ww_kArtMouthC[5] =
{
    "pppppppppp",
    "pppppppppp",
    "pkkkkkkkkp",
    "kppppppppk",
    "pppppppppp"
};

const char *const ww_kArtMouthO[5] =
{
    "ppwwwwwwpp",
    "pjjjjjjjjp",
    "pjjjjjjjjp",
    "pjjjjjjjjp",
    "ppprrrrppp"
};
