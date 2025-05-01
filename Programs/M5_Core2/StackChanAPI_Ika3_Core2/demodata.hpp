#ifndef DEMODATA_H
#define DEMODATA_H

struct MusicInfo {
  const char* filename;
  int intoro;
  int rhythm;
};
const MusicInfo musicList[] = {
  { "banpaku.wav", 6, 500 },
  { "FutureDreams.wav", 10, 700 },
  { "UchuuKomichi.wav", 20, 600 },
  { "Fujinohana.wav", 8, 700 },
  { "Uchujin.wav", 15, 680 },
  { "ramen.wav", 8, 550 },
  { "KawaruEki.wav", 11, 880 },
  { "deepLeaning.wav", 18, 520 },
  { "SouzouSpark.wav", 4, 820 },
  { "HottoHitoiki.wav", 15, 680 },
  { "blackblock.wav", 4, 570 },
  { "KouKyou.wav", 14, 740 },
  { "Kumanoji.wav", 11, 740 },
  { "hajimariRokuoka.wav", 4, 550 },
  { "Kibounoie.wav", 4, 800 },
  { "Harugeshiki.wav", 10, 700 },
  { "Kominka.wav", 8, 600 },
  { "HotcChoco.wav", 10, 680 },
  { "Makenaikokoro.wav", 0, 250 },
  { "nagaraStrech.wav", 15, 500 },
  { "Kittodaijobu.wav", 4, 700 },
  { "Aridanomikan.wav", 14, 500 },
  { "Ankodaisuki.wav", 8, 600 },
  { "Yubaekaerimitchi.wav", 5, 700 },
  { "OnigiriTabeta.wav", 0, 250 },
  { "hatsumode.wav", 11, 750 },
  { "IchigoPafe.wav", 19, 750 },
  { "MiraigaMatteiru.wav", 9, 600 },
  { "Yapparipurin.wav", 0, 250 },
  { "Cppoyakusoku.wav", 15, 650 }
};

const int musicListCount = sizeof(musicList) / sizeof(musicList[0]);

#endif  // DEMODATA_H
