// Okul Kacisi 3D v3 - C++ / SDL3 (2.5D raycasting)
// 12 sinifin her birinde bir defter var. Hepsini topla, sinirli hocadan kac!
// Mudur: koridorda kosarsan veya personel odalarina girersen seni yakalar, ceza odasina atar.
//
// Klavye: W/S ileri-geri, A/D kay, Sol/Sag don, SHIFT kos, E eylem, F esyayi kullan,
//         1-6 slot sec, R yeniden, ESC cikis
// Dokunmatik / fare: sol altta elmas yon tuslari, sag altta KOS / EYLEM / KULLAN,
//         sag ustte 6 slot, bos ekrana parmak surukleyince bakis yonu doner.
//
// Dosyalar: "zeltus" klasoru (oyunun yaninda veya Download/Downloads icinde): 1.jpg ... 11.jpg, door.mp3, git.mp3, defter.mp3, klasik.mp3, soda.mp3, ebar.mp3, skus.mp3, otomat.mp3, vat.mp3, ucak.mp3, kanca.mp3, rehber.mp3, tehlike.mp3, dukkan.mp3, dogru.mp3 (+ 12.jpg ... 16.jpg, 1_3.jpg)
//   1 Kumanda  2 Isimlik  3 Balyoz  4 Kurus  5 EBar  6 Lsoda  7 Yop  8 Lsoda kabarcigi
//
// Derleme (jpg + mp3 icin SDL3_image ve SDL3_mixer varsa otomatik kullanilir):
//   g++ okul3d.cpp -o okul3d $(pkg-config --cflags --libs sdl3 sdl3-image sdl3-mixer)
// Sadece SDL3 varsa:  g++ okul3d.cpp -o okul3d $(pkg-config --cflags --libs sdl3) -DIMG_YOK -DMIX_YOK
#include <SDL3/SDL.h>
#if !defined(IMG_YOK) && __has_include(<SDL3_image/SDL_image.h>)
#include <SDL3_image/SDL_image.h>
#define HAS_IMG 1
#elif !defined(IMG_YOK) && __has_include(<SDL3/SDL_image.h>)
#include <SDL3/SDL_image.h>
#define HAS_IMG 1
#endif
#if !defined(MIX_YOK) && __has_include(<SDL3_mixer/SDL_mixer.h>)
#include <SDL3_mixer/SDL_mixer.h>
#define HAS_MIX 1
#elif !defined(MIX_YOK) && __has_include(<SDL3/SDL_mixer.h>)
#include <SDL3/SDL_mixer.h>
#define HAS_MIX 1
#endif
#include <vector>
#include <map>
#include <string>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <queue>
#include <cstdio>

const int SW = 800, SH = 600;       // ekran (mantiksal)
const int RW = 400, RH = 300;       // 3B cizim cozunurlugu
const int MW = 48, MH = 43;     // MH: guney kanat (6 makine odasi) icin 34'ten 43'e uzatildi
const int ODA_SAYISI = 12;
const int SLOT = 6;
const double PI = 3.14159265358979323846;

// harita hucreleri
enum { T_BOS = 0, T_DUVAR = 1, T_SIRA = 2, T_MAVI = 3, T_MAVI_ACIK = 4, T_SARI = 5, T_SARI_ACIK = 6,
       T_TABLO = 7, T_OTO_E = 8, T_OTO_L = 9, T_OTO_KIRIK = 10, T_KILIT = 11, T_CETVEL = 12 };
// esyalar
enum { E_KUMANDA = 0, E_ISIMLIK, E_BALYOZ, E_KURUS, E_EBAR, E_LSODA, E_YOP, E_SILGI, E_CETVEL, E_UCAK, E_KANCA, E_DIYET, E_MUZ, E_SEKER, E_KALEM, E_SAYI };
const char* esAd[E_SAYI] = {"Kumanda", "Isimlik", "Balyoz", "Kurus", "EBar", "Lsoda", "Yop", "Silgi", "Cetvel", "Ucak", "Kanca", "Diyet Lsoda", "Muz Kabugu", "Naneli Seker", "Kalem"};
const char* esAcik[E_SAYI] = {"ses cikar, Mudur gelir", "30sn Mudur dokunamaz", "otomata bak: 3 adet cikar",
                              "otomattan almak icin", "10sn sinirsiz kosu", "oda/koridorda kabarcik",
                              "30sn kapi/otomat sessiz", "10sn duman: kimse giremez", "sari kapiyi 15sn sikistirir", "kovalarken at: hoca oraya kosar", "baktigin yere cekilirsin", "daha kisa mesafeli kabarcik",
                              "onune at: koridor sonuna kayar", "Velet carpinca kapar, 5sn oyalar", "Nobetci gelince kagidini bozar, kacar"};
const int esFB[E_SAYI][3] = {{200, 60, 60}, {240, 180, 100}, {150, 150, 160}, {190, 190, 205},
                             {110, 80, 80}, {70, 80, 200}, {160, 220, 90}, {235, 235, 235}, {225, 190, 110}, {250, 250, 250},
                             {150, 150, 160}, {200, 230, 255}, {250, 225, 60}, {230, 245, 235}, {240, 200, 60}};
const double esHS[E_SAYI] = {0.38, 0.22, 0.50, 0.20, 0.30, 0.34, 0.22, 0.26, 0.20, 0.22, 0.26, 0.30, 0.18, 0.16, 0.26};      // sprite yuksekligi
double esAsp[E_SAYI] = {0.33, 1.43, 0.43, 0.94, 0.75, 0.50, 1.0, 1.6, 4.0, 1.5, 1.0, 0.5, 1.4, 1.0, 0.4};            // en/boy (resimden guncellenir)

int harita[MH][MW], haritaOrj[MH][MW];
bool koridor[MH][MW], personelH[MH][MW], dolu[MH][MW];
int bfsP[MH][MW], bfsM[MH][MW], bfsI[MH][MW], bfsH[MH][MW];

struct Oda { int x0, y0, kapiY, uzakSatir; bool personel; };
struct Defter { double x, y; bool alindi; bool quiz = false; };   // quiz: akilli tahtali baslangic defteri
struct Esya { double x, y; int tip; bool alindi; double lift; };
struct Sp { double x, y; int tip, id; double d; };      // tip: 0 defter, 1 hoca, 2 esya, 3 mudur, 4 kabarcik, 5 muhbir, 6 velet
struct Seg { int x0, y0, x1, y1; bool yatay; };
struct SariKapi { int x, y; bool acik; bool kilitli = false; double sikis = 0; };   // sikis: cetvel sikisma suresi
struct Kabarcik { bool var = false; int bx0 = 0, by0 = 0, bx1 = 0, by1 = 0; bool diyet = false; double x = 0, y = 0, ax = 0, ay = 0, kalan = 0; bool h = false, m = false, o = false; };

std::vector<Oda> odalar;
std::vector<Defter> defterler;
std::vector<Esya> esyalar;
std::vector<Seg> segler;
std::vector<SariKapi> sariKapilar;
Kabarcik kab;

double px, py, dx, dy, plx, ply;     // oyuncu, bakis yonu, kamera duzlemi
double hx, hy;                       // hoca
double ux, uy;                       // mudur
int toplanan, durum;                 // durum: 0 oynuyor, 1 kazandi, 2 yakalandi

// ===== baslangic: akilli tahta + kovalamaca =====
// ===== HILE (hizli test): TEK ANAHTAR. true = baslangicta 9999 MS + esya bolluğu + dolu envanter, false = kapali. YAYINLARKEN false YAP! =====
// (Kanca bunun disinda: her zaman baslangicta cantadadir. Hile kodu yeniden() fonksiyonunun sonundaki "HILE" blogudur.)
const bool HILE = false;
bool girisMenu = false;               // true: giris menusu acik (Oyna / Rehber). Oyna'ya basinca oyun baslar
const SDL_FRect btnMOyna = {SW - 330.f, 200.f, 280.f, 90.f};
const SDL_FRect btnMRehber = {SW - 330.f, 320.f, 280.f, 90.f};
// ---- REHBER ekrani: solda esya gorselleri, sagda ne ise yaradigi, en sagda kaydirma cubugu ----
bool rehberAcik = false;
double rehberKay = 0;                // kaydirma miktari (px)
int rehberSurukle = 0;               // 0 yok, 1 icerigi surukleme, 2 kaydirma cubugunu surukleme
float rehberSonY = 0;
const float REH_LIST_Y = 70.f, REH_LIST_H = 520.f, REH_ROW = 112.f, REH_SB_X = SW - 28.f;
const SDL_FRect btnRMenu = {SW - 170.f, 12.f, 150.f, 46.f};      // en ust sagda "MENU" dugmesi
static const std::vector<std::string> REH_YAZI[15] = {      // esya sirasi: enum sirasi (E_KUMANDA ... E_KALEM)
    {"Ses cikarir, Mudur sesin geldigi yere kosar.", "Mudur'u baska yone cekmek icin kullan."},
    {"Takinca 30 sn Mudur sana dokunamaz.", "Sure bitince Mudur yine yakalar."},
    {"Otomata bakip kullan: otomat parcalanir ve", "icinden 3 adet urun cikar. Ama gurultu olur,", "hoca hizlanir (Yop varsa sessiz kalir)."},
    {"Otomattan EBar veya Lsoda almak icin gerekir.", "Koridorlarda yerde bulunur."},
    {"Kullaninca 10 sn sinirsiz kosarsin,", "kosu enerjin bitmez."},
    {"Koridorda kullan: kabarcik ilerler. Icine giren", "hoca ve Mudur surulur, sonunda sasirir.", "Sen de icine girersen suruklenirsin."},
    {"30 sn boyunca kapi ve otomat sesi cikmaz,", "boylece hoca gurultuyle hizlanmaz."},
    {"Baktigin yone 10 sn beyaz duman birakir.", "Hoca, Mudur, Muhbir ve Velet dumana giremez."},
    {"Sari kapiya yaklas, ona bak ve kullan:", "kapi 15 sn sikisir, kimse gecemez.", "Kapida biri varsa kullanilamaz."},
    {"Kovalamaca sirasinda at: dustugu yere hoca", "8 sn kosar. Muhbir o sure ustune yazamaz."},
    {"Baktigin yone firlatilir (en fazla 14 karo).", "Duvara kadar gider ve seni hizla oraya ceker."},
    {"Lsoda ile ayni isi yapar ama kabarcik daha", "kisa mesafe gider (en fazla 6 karo)."},
    {"Onune atilir (en fazla 2.5 karo). Hoca, Mudur,", "Muhbir veya Velet basarsa kayar ve sasirir.", "Kabugu yerden geri alabilirsin."},
    {"Sec: Velet sana carpinca sekeri kapar,", "5 sn mutlu olup oyalanir, sen kacarsin.", "Veleti gorunce KULLAN ile de verebilirsin."},
    {"Sec: Nobetci Ogrenci yanina gelince kagidini", "bozar, uzgun uzaklasir, refleks testi olmaz.", "Bedel: sonraki testler 1 sn yerine 0.7 sn."}
};
const bool TEST_MOD = true;          // karakterler (velet / kopyaci / nobetci) kilitli kapinin otesinde baslar. YAYINLARKEN false YAP!
const int KOVA_UZAKLIK = 40;         // kovalamaca baslarken hoca oyuncudan en az bu kadar karo (yol uzakligi) uzakta olur
const double TAHTA_AT_SURE = 1.5;    // yanlis cevaptan sonra tahtada kalma suresi (sn), sonra disari atilirsin
// ===== HOCA HIZI AYARLARI (karo/sn; yuruyus 3.0, kosu 5.0) =====
const double HOCA_TEMEL_HIZ = 0.9;       // kovalamaca basinda hocanin hizi
const double HOCA_DEFTER_ARTIS = 0.08;   // toplanan her defter icin eklenen hiz
const double HOCA_YANLIS_ARTIS = 0.15;   // her yanlis cevapta hiz bu oranda artar (0.15 = %15)
const double HOCA_MAX_HIZ = 3.6;         // hoca bundan hizli olamaz
const double KLASIK_MAX = 4.0;       // klasik.mp3 en fazla bu kadar calar, sonra hoca kovalamaya baslar
int kovaDurum = 0;                   // 0 hoca uyuyor, 1 klasik.mp3 (hoca henuz durur), 2 kovaliyor
double kovaSure = 0;
int yanlisSayi = 0;                  // her yanlis cevapta hoca hizlanir
int cozulenDefter = 0;               // akilli tahtada biten defter sayisi (0-2)
bool girisKilitli = true;            // baslangic koridorunun sonundaki kapi
bool quizAtla = false;               // bir kere yanlis yapilinca true: baslangic defterleri tahtasiz, dogrudan toplanir
struct Tahta {
    bool acik = false;
    int defterId = -1, defterNo = 0, soru = 0, dogruIdx = 0;
    bool pi = false;
    std::string metin, sec[3], mesaj;
    bool elendi[3] = {false, false, false};
    bool yesil[3] = {false, false, false};
    double atSure = 0;                  // >0: yanlis cevap verildi, bu kadar sn sonra tahtadan atilirsin
};
Tahta tahta;
double sure, hocaDonuk, hocaHizli, hKapi;
double uAlarm, uDonuk, uKapi, uwx, uwy;
double mhx, mhy, mhKapi, mhwx, mhwy;     // muhbir: hizli, ince, iki kafali
double ihbarSure, ihbarTekrar, ihbarX, ihbarY;   // muhbirin hocaya bildirdigi konum
// ===== VELET: yavas gezer; seni ayni koridorda gorunce uzerine kosar, carpinca koridorun sonuna goturur =====
const double VELET_GEZ_HIZ = 0.9;    // gezinme hizi (karo/sn)
const double VELET_KOS_HIZ = 3.3;    // seni gorunce kosma hizi (yuruyus 3.0, kosu 5.0)
const double VELET_TASI_HIZ = 7.0;   // seni goturme hizi
const double VELET_GORUS = 12.0;     // en fazla bu kadar karodan gorur
const double VELET_DINLEN = 8.0;     // birakildiktan sonra bu kadar sn tekrar saldirmaz
double vlx, vly, vKapi, vwx, vwy, vDinlen;
const double SEKER_SURE = 5.0;       // naneli seker: velet bu kadar sn oyalanir (menzil: VELET_GORUS, gorus acik olmali)
double veletOyalan = 0;              // >0: velet sekere dalmis, kimseyi kovalamaz / tasimaz
// muz kabugu: yere atilir (yerdeki esya); hoca / mudur / muhbir / velet yaklasinca kabuk kaybolur, hareket yonunde duvara kadar kayar
struct Kay { bool var = false; double ax = 0, ay = 0, kalan = 0; };
Kay kayH, kayU, kayM, kayV;          // kayan hoca / mudur / muhbir / velet
const double MUZ_MENZIL = 2.5;       // kabuk senin onune en fazla bu kadar karo uzaga duser
const double MUZ_TETIK = 1.05;       // dusman kabuga bu kadar yaklasinca kayar (koridor 2 karo genis)
const double MUZ_KAYMA_HIZ = 8.0;    // kayma hizi (karo/sn)
const double MUZ_SASIR = 3.0;        // kayma bitince hoca/mudur/velet bu kadar sn sasirir
bool veletKos, veletTasi;
int vSeg;                            // gorus/tasima koridoru (segler indeksi)
double vTasiAx, vTasiAy, vTasiKalan; // tasima yonu ve kalan mesafe
int bfsV[MH][MW];
// ===== NOBETCI OGRENCI: gozluklu, okul formali, elinde kagit. Seni gorunce gelir, yakalayinca REFLEKS TESTI =====
// Kagidin sol altinda YESIL, sag altinda MAVI kutu var. 5-6 sn sonra kagidin ustunde bir rengin adi yazar,
// 1 sn icinde o renge basmazsan test tekrarlanir; 5. basarisizlikta nobetci gider. Gelirken vat.mp3 calar.
const double NOBET_HIZ_GEZ = 1.6, NOBET_HIZ_GEL = 2.6;   // gezinme / yaklasma hizi
const double NOBET_GORUS = 11.0;                          // en fazla bu kadar karodan gorur
const double NOBET_TEPKI = 1.0;                           // ismi gorunce basma suresi (sn)
const double NOBET_BEKLE_MIN = 5.0, NOBET_BEKLE_MAX = 6.0;// isim cikmadan once bekleme (sn)
const double NOBET_DINLEN = 25.0;                         // gittikten sonra tekrar gelmeme suresi
const int NOBET_HAK = 5;                                  // 5. basarisizlikta gider
double nbx, nby, nbKapi, nbwx, nbwy, nbDinlen, nbKayip, nbT, nbBekle;
int nbMod;        // 0 gezinme, 1 yaklasiyor, 2 refleks testi
int nbFaz;        // 0 kagit kalkiyor, 1 bekleme, 2 isim gorunur, 3 sonuc mesaji
int nbHedef;      // 0 yesil, 1 mavi
int nbBasarisiz;
std::string nbMesaj;
int bfsN[MH][MW];
// KALEM: secili iken nobetci yanina gelirse kagidini bozar, uzgun uzaklasir; bedel: refleks testi suresi kalici 0.7 sn
const double NOBET_TEPKI_KALEM = 0.7;                     // kalem kullanildiktan sonra ismi gorunce basma suresi (sn)
const double NOBET_HIZ_KAC = 4.2;                         // kalemden sonra uzaklasma hizi
const double NOBET_KAC_SURE = 5.0, NOBET_ETIKET_SURE = 3.0;
double nbTepki = NOBET_TEPKI;                             // gecerli tepki suresi (kalemden sonra 0.7)
bool nbUzgun;                                             // true: uzgun yuz + inik gozluk (kalici, yeniden baslayana kadar)
double nbKacT, nbEtiketT, nbKacX, nbKacY;                 // nbMod 3: uzaklasma sayaci, yazi sayaci, kacis hedefi
double nbEkX, nbEkUst; bool nbEkGor;                      // nobetcinin ekran konumu (yazi icin)
// ===== KOPYACI COCUK: envanterinden bir esya calip kacar; yakalarsan geri alirsin. Yaklasirken tehlike.mp3 calar =====
const double KP_HIZ_GEZ = 1.8, KP_HIZ_GEL = 3.3, KP_HIZ_KAC = 3.8;   // yuruyus 3.0, kosu 5.0
const double KP_GORUS = 12.0;            // en fazla bu kadar karodan seni gorur (envanterinde esya varsa)
const double KP_TEHLIKE_MESAFE = 7.0;    // bu kadar yaklasinca tehlike.mp3 calar
const double KP_KACIS_SURE = 30.0;       // bu sure icinde yakalayamazsan esyayi bir yere birakip kaybolur
const double KP_DINLEN = 40.0;           // ayni cocuk tekrar saldirmadan once bekleme
double kpx, kpy, kpKapi, kpwx, kpwy, kpDinlen, kpKayip, kpT, kpWpT, kpfx, kpfy;
int kpMod;        // 0 gezinme, 1 yaklasiyor, 2 esyayla kaciyor
int kpTip;        // calinan esya turu (-1: yok)
bool kpTehlikeCaldi;
bool kpMuzik;                            // true iken tehlike.mp3 calar; kopyaci biraz uzaklasinca hemen susar
const double KP_TEHLIKE_BIRAK = 7.5;     // bu mesafeyi gecince muzik biter (baslama mesafesinden azicik buyuk: titreme olmasin)
int bfsK[MH][MW];
// ===== MAKINE ODALARI (yesil kapi): makinedeki islemin sonucuna esit balonu makineye ver -> +25 Makine Soru (MS) =====
// 7 oda var (haritaKur). Her biri 5x5; yesil kapi ust duvarda (x0+1, y0-1), makine ust duvarin yaninda (x0+2.5, y0+0.75).
// Her odanin kendi sorusu ve kendi balonlari var; dogru cevap verilince o odanin sorusu yenilenir.
struct MakOda { int x0 = 0, y0 = 0; double mx = 0, my = 0, bx0 = 0, bx1 = 0, by0 = 0, by1 = 0; std::string metin; int cevap = 0; };
std::vector<MakOda> makOdalar;                                               // haritaKur() doldurur
const int MS_PUAN = 25;                                                      // dogru cevap basina verilen puan
bool yesilH[MH][MW];                                                         // yesil boyanan kapi hucreleri
int msPuan;                                                                  // Makine Soru puani
int elBalon;                                                                 // elimizdeki balonun sayisi (-1: yok)
struct Balon { double x, y, vx, vy; int deger; double faz; int oda = 0; };   // oda: hangi makine odasinin balonu
std::vector<Balon> balonlar;
const unsigned char FONT35[10][5] = {{7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},
                                     {7,4,7,1,7},{7,4,7,5,7},{7,1,1,1,1},{7,5,7,5,7},{7,5,7,1,7}};   // 3x5 rakamlar (balon etiketi)
// ===== KANCA (12.jpg): baktigin yone firlatilir, seni o yere ceker =====
struct Kanca { bool var = false; double tx = 0, ty = 0; };
Kanca kanca;
const double KANCA_MENZIL = 14.0, KANCA_HIZ = 14.0;   // en uzak menzil (karo) ve cekilme hizi (karo/sn)
// ===== KANTINCI TEYZE (kafeterya): Makine Soru puani (MS) karsiligi esya satar =====
const double KANTINCI_X = 43.5, KANTINCI_Y = 16.5;               // kafeteryada tezgah
bool kantinAcik;
std::string kantinMesaj; double kantinMesajT;
const int KANTIN_URUN[5] = {E_KANCA, E_DIYET, E_MUZ, E_SEKER, E_KALEM};
const int KANTIN_FIYAT[5] = {100, 10, 25, 15, 25};               // MS
SDL_FRect kantinSatir(int i) { return {80, 96.0f + i * 84, 640, 76}; }
SDL_FRect kantinGeri() { return {SW / 2 - 90.0f, 522, 180, 56}; }
void makineSoruUret(int k) {               // k. makine odasi: tek sayilarla carpma, cikarma, bolme
    MakOda& m = makOdalar[k];
    int tur = std::rand() % 3;
    const int t[4] = {3, 5, 7, 9};
    if (tur == 0) {                        // carpma: tek x tek
        int a = t[std::rand() % 4], b = t[std::rand() % 4];
        m.cevap = a * b; m.metin = std::to_string(a) + " x " + std::to_string(b) + " = ?";
    } else if (tur == 1) {                 // cikarma: tek - tek
        int a = 2 * (5 + std::rand() % 11) + 1;                       // 11..31 (tek)
        int b = 3 + 2 * (std::rand() % ((a - 3) / 2));                // 3..a-2 (tek)
        m.cevap = a - b; m.metin = std::to_string(a) + " - " + std::to_string(b) + " = ?";
    } else {                               // bolme: (tek x tek) / tek
        int b = t[std::rand() % 4], q = t[std::rand() % 4];
        m.cevap = q; m.metin = std::to_string(b * q) + " / " + std::to_string(b) + " = ?";
    }
}
void balonlariUret(int k) {                // k. odanin balonlari yenilenir: 1 dogru + 7 yanlis
    const MakOda& m = makOdalar[k];
    balonlar.erase(std::remove_if(balonlar.begin(), balonlar.end(), [k](const Balon& b) { return b.oda == k; }), balonlar.end());
    std::vector<int> v = {m.cevap};
    for (int guard = 0; (int)v.size() < 8 && guard < 300; guard++) {
        int d = m.cevap + (std::rand() % 21) - 10;
        if (d < 1 || d > 99 || std::find(v.begin(), v.end(), d) != v.end()) continue;
        v.push_back(d);
    }
    for (int deger : v) {
        Balon b;
        b.oda = k;
        b.x = m.bx0 + (m.bx1 - m.bx0) * (std::rand() % 1000) / 1000.0;
        b.y = (m.by0 + 0.8) + (m.by1 - m.by0 - 0.8) * (std::rand() % 1000) / 1000.0;
        double ang = (std::rand() % 628) / 100.0;
        b.vx = std::cos(ang) * 0.35; b.vy = std::sin(ang) * 0.35;
        b.deger = deger; b.faz = (std::rand() % 628) / 100.0;
        balonlar.push_back(b);
    }
}
void balonGuncelle(double dt) {            // balonlar kendi odalarinda yavasca suzulur
    for (auto& b : balonlar) {
        const MakOda& m = makOdalar[b.oda];
        b.faz += dt;
        b.vx += std::cos(b.faz * 1.7) * 0.25 * dt; b.vy += std::sin(b.faz * 1.3) * 0.25 * dt;
        double sp = std::hypot(b.vx, b.vy);
        if (sp > 0.5) { b.vx *= 0.5 / sp; b.vy *= 0.5 / sp; }
        b.x += b.vx * dt; b.y += b.vy * dt;
        if (b.x < m.bx0) { b.x = m.bx0; b.vx = std::fabs(b.vx); }
        if (b.x > m.bx1) { b.x = m.bx1; b.vx = -std::fabs(b.vx); }
        if (b.y < m.by0) { b.y = m.by0; b.vy = std::fabs(b.vy); }
        if (b.y > m.by1) { b.y = m.by1; b.vy = -std::fabs(b.vy); }
    }
}
// ===== KAGIT UCAK (11.jpg, ucak.mp3): bakilan yone ucar; dustugu yere hoca kosar (sadece hoca kovalarken) =====
struct Ucak { bool var = false; double x = 0, y = 0, ax = 0, ay = 0, kalan = 0; };
Ucak ucak;
const double UCAK_HIZ = 9.0, UCAK_MESAFE = 10.0;   // ucus hizi (karo/sn) ve en uzun menzil
const double UCAK_LURE = 8.0;                      // hoca dustugu yere bu kadar sn kosar
inline bool nobetMuzik() { return nbMod == 1 || nbMod == 2; }   // true iken vat.mp3 calar, kovalamaca muzigi susar
inline bool digerMuzikSus() { return nobetMuzik() || kantinAcik; }   // kovalamaca / klasik muzigi susar
// ===== SILGI (9.jpg): kullanilan yerde 10sn beyaz duman; hoca/mudur/muhbir/velet icine GIREMEZ =====
struct Duman { double x, y, sure; };
std::vector<Duman> dumanlar;
const double DUMAN_R = 1.7, DUMAN_SURE = 10.0;
const double CETVEL_SURE = 15.0;     // cetvel sikisinca kapi bu kadar sn kullanilamaz
bool dumanIcinde(int cx, int cy) {     // bu karonun merkezi dumanin icinde mi?
    for (const auto& d : dumanlar) if (std::hypot(cx + 0.5 - d.x, cy + 0.5 - d.y) < DUMAN_R) return true;
    return false;
}
int cezaSayi;
double kosuEnerji, sonsuzSure, isimlikSure, yopSure, kumandaSure, kumandaX, kumandaY, cezaKalan, bildirimSure;
bool yorgun, kosuyor;
std::string bildirim;
int slot[SLOT], adet[SLOT];          // slot: -1 bos, yoksa esya tipi
int sel = -1;
int hedefTip = -1, hedefId = -1;     // 0 defter, 2 esya, 3 mavi kapi, 4 otomat
int hedefKX = 0, hedefKY = 0;
double hedefDist = 99;

static inline Uint32 rgb(int r, int g, int b) {
    r = r < 0 ? 0 : (r > 255 ? 255 : r);
    g = g < 0 ? 0 : (g > 255 ? 255 : g);
    b = b < 0 ? 0 : (b > 255 ? 255 : b);
    return 0xFF000000u | ((Uint32)r << 16) | ((Uint32)g << 8) | (Uint32)b;
}
static inline int kanal(Uint32 c, int sh) { return (int)((c >> sh) & 255u); }
static inline void karistir(Uint32& p, int R, int G, int B, int a) {
    int pr = kanal(p, 16), pg = kanal(p, 8), pb = kanal(p, 0);
    p = rgb((pr * (255 - a) + R * a) / 255, (pg * (255 - a) + G * a) / 255, (pb * (255 - a) + B * a) / 255);
}
std::vector<Uint32> pix(RW * RH);

bool gecilir(int h) { return h == T_BOS || h == T_MAVI || h == T_MAVI_ACIK || h == T_SARI || h == T_SARI_ACIK; }

void bildir(const std::string& s) { bildirim = s; bildirimSure = 3.0; }

// ================= DOSYA BULMA =================
// "zeltus" klasorunu ve Download/Downloads klasorlerini sirayla arar.
std::vector<std::string> aramaKlasorleri;
std::string acilisUyari;
std::vector<std::string> tani;      // acilista ekranda gosterilen durum satirlari
double taniSure = 0;
std::string ilkResimHata, mixHata;

void klasorleriHazirla() {
    std::vector<std::string> kok;
    kok.push_back("");
    if (const char* bp = SDL_GetBasePath()) { kok.push_back(bp); kok.push_back(std::string(bp) + "../"); }
    const char* ev = std::getenv("HOME");
    const char* adlar[] = {"Download/", "Downloads/", "download/", "downloads/"};
    std::vector<std::string> tabanlar = {"/sdcard/", "/storage/emulated/0/", "/storage/self/primary/",
                                         "/storage/emulated/0/Android/media/"};
    if (ev) {
        std::string h = std::string(ev) + "/";
        tabanlar.push_back(h);
        tabanlar.push_back(h + "storage/shared/");
        tabanlar.push_back(h + "storage/");
    }
    for (auto& k : kok) { aramaKlasorleri.push_back(k + "zeltus/"); aramaKlasorleri.push_back(k + "Zeltus/"); }
    for (auto& t : tabanlar)
        for (auto a : adlar) {
            aramaKlasorleri.push_back(t + a + "zeltus/");
            aramaKlasorleri.push_back(t + a + "Zeltus/");
            aramaKlasorleri.push_back(t + a);                 // dosyalar dogrudan Download'da ise
        }
    for (auto a : adlar) { aramaKlasorleri.push_back(std::string(a) + "zeltus/"); aramaKlasorleri.push_back(a); }
    aramaKlasorleri.push_back("");                            // calisma klasoru
}

// ================= GOMULU DOSYALAR (tek dosya surumu) =================
// -DGOMULU ile derlenirse zeltus klasorundeki resim ve muzikler programin icine gomulur (.incbin).
// -DASSET_DIR='"yol/zeltus"' gomulecek klasoru gosterir. -DGOMULU yoksa dosyalar eskisi gibi klasorlerden okunur.
#ifdef GOMULU
#ifndef ASSET_DIR
#define ASSET_DIR "zeltus"
#endif
#define GOM(id, dosya) \
    __asm__(".pushsection .rodata\n.balign 16\n.global gom_" #id "\ngom_" #id ":\n.incbin \"" ASSET_DIR "/" dosya "\"\n" \
            ".global gom_" #id "_son\ngom_" #id "_son:\n.byte 0\n.popsection\n"); \
    extern "C" const unsigned char gom_##id[]; extern "C" const unsigned char gom_##id##_son[];
GOM(f0, "1.jpg")
GOM(f1, "10.jpg")
GOM(f2, "11.jpg")
GOM(f3, "12.jpg")
GOM(f4, "13.jpg")
GOM(f5, "14.jpg")
GOM(f6, "15.jpg")
GOM(f7, "16.jpg")
GOM(f8, "1_3.jpg")
GOM(f9, "2.jpg")
GOM(f10, "3.jpg")
GOM(f11, "4.jpg")
GOM(f12, "5.jpg")
GOM(f13, "6.jpg")
GOM(f14, "7.jpg")
GOM(f15, "8.jpg")
GOM(f16, "9.jpg")
GOM(f17, "defter.mp3")
GOM(f18, "dogru.mp3")
GOM(f19, "door.mp3")
GOM(f20, "dukkan.mp3")
GOM(f21, "ebar.mp3")
GOM(f22, "git.mp3")
GOM(f23, "kanca.mp3")
GOM(f24, "klasik.mp3")
GOM(f25, "otomat.mp3")
GOM(f26, "rehber.mp3")
GOM(f27, "skus.mp3")
GOM(f28, "soda.mp3")
GOM(f29, "tehlike.mp3")
GOM(f30, "ucak.mp3")
GOM(f31, "vat.mp3")
struct GomKayit { const char* ad; const unsigned char* bas; const unsigned char* son; };
static const GomKayit gomTablo[] = {
    {"1.jpg", gom_f0, gom_f0_son},
    {"10.jpg", gom_f1, gom_f1_son},
    {"11.jpg", gom_f2, gom_f2_son},
    {"12.jpg", gom_f3, gom_f3_son},
    {"13.jpg", gom_f4, gom_f4_son},
    {"14.jpg", gom_f5, gom_f5_son},
    {"15.jpg", gom_f6, gom_f6_son},
    {"16.jpg", gom_f7, gom_f7_son},
    {"1_3.jpg", gom_f8, gom_f8_son},
    {"2.jpg", gom_f9, gom_f9_son},
    {"3.jpg", gom_f10, gom_f10_son},
    {"4.jpg", gom_f11, gom_f11_son},
    {"5.jpg", gom_f12, gom_f12_son},
    {"6.jpg", gom_f13, gom_f13_son},
    {"7.jpg", gom_f14, gom_f14_son},
    {"8.jpg", gom_f15, gom_f15_son},
    {"9.jpg", gom_f16, gom_f16_son},
    {"defter.mp3", gom_f17, gom_f17_son},
    {"dogru.mp3", gom_f18, gom_f18_son},
    {"door.mp3", gom_f19, gom_f19_son},
    {"dukkan.mp3", gom_f20, gom_f20_son},
    {"ebar.mp3", gom_f21, gom_f21_son},
    {"git.mp3", gom_f22, gom_f22_son},
    {"kanca.mp3", gom_f23, gom_f23_son},
    {"klasik.mp3", gom_f24, gom_f24_son},
    {"otomat.mp3", gom_f25, gom_f25_son},
    {"rehber.mp3", gom_f26, gom_f26_son},
    {"skus.mp3", gom_f27, gom_f27_son},
    {"soda.mp3", gom_f28, gom_f28_son},
    {"tehlike.mp3", gom_f29, gom_f29_son},
    {"ucak.mp3", gom_f30, gom_f30_son},
    {"vat.mp3", gom_f31, gom_f31_son},
};
static const GomKayit* gomBul(const std::string& ad) {
    for (auto& g : gomTablo) if (ad == g.ad) return &g;
    return nullptr;
}
#endif

// ad ornek: "1.jpg". Bulursa tam yolu, bulamazsa "" dondurur. Gomulu dosya varsa "mem:ad" dondurur.
std::string dosyaBul(const std::string& ad) {
#ifdef GOMULU
    if (gomBul(ad)) return "mem:" + ad;
#endif
    for (auto& k : aramaKlasorleri) {
        std::string yol = k + ad;
        if (FILE* f = std::fopen(yol.c_str(), "rb")) { std::fclose(f); return yol; }
    }
    return "";
}

// ================= RESIMLER =================
struct Img { int w = 0, h = 0; std::vector<Uint32> p; SDL_Texture* t = nullptr; bool ok = false; };
const int IMG_KAB = E_SAYI;          // 8.jpg: lsoda kabarcigi (esya degil, ayri resim)
const int IMG_DKAB = E_SAYI + 1;     // 1_3.jpg: diyet lsoda kabarcigi
Img img[E_SAYI + 2];
int resimEksik = 0;

// "mem:ad" ise gomulu veriden, degilse dosyadan yukler
SDL_Surface* resimAc(const std::string& yol) {
#ifdef GOMULU
    if (yol.rfind("mem:", 0) == 0) {
        if (const GomKayit* g = gomBul(yol.substr(4))) {
            SDL_IOStream* io = SDL_IOFromConstMem(g->bas, (size_t)(g->son - g->bas));
            if (!io) return nullptr;
#ifdef HAS_IMG
            return IMG_Load_IO(io, true);
#else
            return SDL_LoadBMP_IO(io, true);
#endif
        }
    }
#endif
#ifdef HAS_IMG
    return IMG_Load(yol.c_str());
#else
    return SDL_LoadBMP(yol.c_str());
#endif
}
#ifdef HAS_MIX
MIX_Audio* sesAc(MIX_Mixer* m, const std::string& yol) {
#ifdef GOMULU
    if (yol.rfind("mem:", 0) == 0) {
        if (const GomKayit* g = gomBul(yol.substr(4))) {
            SDL_IOStream* io = SDL_IOFromConstMem(g->bas, (size_t)(g->son - g->bas));
            return io ? MIX_LoadAudio_IO(m, io, false, true) : nullptr;
        }
    }
#endif
    return MIX_LoadAudio(m, yol.c_str(), false);
}
#endif

void resimleriYukle(SDL_Renderer* r) {
    for (int t = 0; t < E_SAYI + 2; t++) {
        // dosya adlari: Silgi=9, Cetvel=10, Ucak=11, Kanca=12, Diyet Lsoda=13, Muz=14, Seker=15, Kalem=16, kabarcik=8, diyet kabarcigi=1_3
        std::string ad = (t == IMG_DKAB) ? std::string("1_3") : std::to_string(
            t == E_KALEM ? 16 : (t == E_SEKER ? 15 : (t == E_MUZ ? 14 : (t == E_DIYET ? 13 : (t == E_KANCA ? 12 :
            (t == E_UCAK ? 11 : (t == E_CETVEL ? 10 : (t == E_SILGI ? 9 : (t == IMG_KAB ? 8 : t + 1)))))))));
        SDL_Surface* s = nullptr;
        bool bulundu = false;
        const char* uzanti[] = {".jpg", ".jpeg", ".JPG", ".png", ".bmp"};
        for (auto u : uzanti) {
            std::string yol = dosyaBul(ad + u);
            if (yol.empty()) continue;
            bulundu = true;
            s = resimAc(yol);
            if (s) break;
            if (ilkResimHata.empty()) ilkResimHata = ad + u + ": " + SDL_GetError();
        }
        if (!s) {
            resimEksik++;
            if (!bulundu && ilkResimHata.empty()) ilkResimHata = ad + ".jpg dosyasi bulunamadi";
            std::fprintf(stderr, "Resim yuklenemedi: %s (%s)\n", ad.c_str(), bulundu ? "bulundu ama acilamadi" : "dosya bulunamadi");
            continue;
        }
        SDL_Surface* c = SDL_ConvertSurface(s, SDL_PIXELFORMAT_ARGB8888);
        SDL_DestroySurface(s);
        if (!c) continue;
        int w = c->w, h = c->h;
        std::vector<Uint32> p((size_t)w * h);
        for (int y = 0; y < h; y++) std::memcpy(&p[(size_t)y * w], (Uint8*)c->pixels + (size_t)y * c->pitch, (size_t)w * 4);
        SDL_DestroySurface(c);
        // kenardan baslayarak beyaz arka plani seffaf yap
        auto beyaz = [&](Uint32 q) { return kanal(q, 16) >= 215 && kanal(q, 8) >= 215 && kanal(q, 0) >= 215; };
        std::vector<char> gor((size_t)w * h, 0);
        std::vector<int> kuy;
        auto ekle = [&](int x, int y) {
            int i = y * w + x;
            if (!gor[i] && beyaz(p[i])) { gor[i] = 1; kuy.push_back(i); }
        };
        for (int x = 0; x < w; x++) { ekle(x, 0); ekle(x, h - 1); }
        for (int y = 0; y < h; y++) { ekle(0, y); ekle(w - 1, y); }
        for (size_t b = 0; b < kuy.size(); b++) {
            int i = kuy[b], x = i % w, y = i / w;
            p[i] = 0;
            if (x > 0) ekle(x - 1, y);
            if (x < w - 1) ekle(x + 1, y);
            if (y > 0) ekle(x, y - 1);
            if (y < h - 1) ekle(x, y + 1);
        }
        // 8.jpg (kabarcik): kenardan ulasilamayan ic beyazlar dahil TUM beyazlari seffaf yap
        if (t == IMG_KAB || t == IMG_DKAB) {
            for (size_t i = 0; i < p.size(); i++)
                if (p[i] != 0 && kanal(p[i], 16) >= 200 && kanal(p[i], 8) >= 200 && kanal(p[i], 0) >= 200) p[i] = 0;
        }
        // en fazla 160 piksele kucult (pikselli kalir); kabarcik daha keskin kalsin diye 256
        int lim = (t == IMG_KAB || t == IMG_DKAB) ? 256 : 160;
        int mxd = std::max(w, h), nw = w, nh = h;
        if (mxd > lim) { nw = std::max(1, w * lim / mxd); nh = std::max(1, h * lim / mxd); }
        std::vector<Uint32> q((size_t)nw * nh);
        for (int ny = 0; ny < nh; ny++)
            for (int nx = 0; nx < nw; nx++)
                q[(size_t)ny * nw + nx] = p[(size_t)((int)((ny + 0.5) * h / nh)) * w + (int)((nx + 0.5) * w / nw)];
        img[t].w = nw; img[t].h = nh; img[t].p = q; img[t].ok = true;
        if (t < E_SAYI) esAsp[t] = (double)nw / nh;
        SDL_Surface* ys = SDL_CreateSurfaceFrom(nw, nh, SDL_PIXELFORMAT_ARGB8888, img[t].p.data(), nw * 4);
        if (ys) {
            img[t].t = SDL_CreateTextureFromSurface(r, ys);
            SDL_DestroySurface(ys);
            if (img[t].t) {
                SDL_SetTextureBlendMode(img[t].t, SDL_BLENDMODE_BLEND);
                SDL_SetTextureScaleMode(img[t].t, SDL_SCALEMODE_NEAREST);
            }
        }
    }
}

// resimden renk ornekle (u,v: 0..1). false = seffaf
bool ornek(int t, double u, double v, int& R, int& G, int& B) {
    Img& m = img[t];
    if (m.ok) {
        int x = (int)(u * m.w), y = (int)(v * m.h);
        x = std::clamp(x, 0, m.w - 1); y = std::clamp(y, 0, m.h - 1);
        Uint32 c = m.p[(size_t)y * m.w + x];
        if ((c >> 24) == 0) return false;
        R = kanal(c, 16); G = kanal(c, 8); B = kanal(c, 0);
        return true;
    }
    bool kenar = u < 0.1 || u > 0.9 || v < 0.06 || v > 0.94;
    int k = kenar ? 5 : 10;
    R = esFB[t][0] * k / 10; G = esFB[t][1] * k / 10; B = esFB[t][2] * k / 10;
    return true;
}

// ================= SES =================
SDL_AudioStream* sesAkis = nullptr;
#ifdef HAS_MIX
MIX_Mixer* mikser = nullptr;
MIX_Audio* kapiMp3 = nullptr;
MIX_Audio* gitMp3 = nullptr;
MIX_Audio* sodaMp3 = nullptr;      // Lsoda kullaninca
MIX_Audio* ebarMp3 = nullptr;      // EBar kullaninca
MIX_Audio* skusMp3 = nullptr;      // Yop kullaninca
MIX_Audio* otomatMp3 = nullptr;    // otomattan bir sey alinca
MIX_Audio* vatMp3 = nullptr;       // nobetci ogrenci gelirken
MIX_Audio* ucakMp3 = nullptr;      // kagit ucak atilinca
MIX_Audio* kancaMp3 = nullptr;     // kanca atilinca
MIX_Audio* tehlikeMp3 = nullptr;   // kopyaci cocuk yaklasirken
MIX_Audio* dogruMp3 = nullptr;     // makine / akilli tahta: dogru cevap
MIX_Track* tehlikeTrack = nullptr;
MIX_Track* vatTrack = nullptr;
MIX_Audio* dukkanMp3 = nullptr;    // kantinde alisveris yaparken
MIX_Track* dukkanTrack = nullptr;
MIX_Track* gitTrack = nullptr;
MIX_Audio* defterMp3 = nullptr;
MIX_Audio* klasikMp3 = nullptr;
MIX_Track* klasikTrack = nullptr;
MIX_Track* defterTrack = nullptr;   // akilli tahta sarkisi (durdurulabilsin diye ayri track)
MIX_Audio* rehberMp3 = nullptr;     // rehber ekraninda calar
MIX_Track* rehberTrack = nullptr;
#endif
bool gitCaliyor = false;
bool klasikOynadi = false;       // klasik.mp3 gercekten calmaya basladi mi
// ACILIS MUZIGI: oyun baslarken klasik.mp3 calar. Akilli tahta acilinca veya kovalamaca baslayinca
// kalici olarak susar (yeniden baslayana kadar tekrar calmaz).
bool acilisMuzik = false;        // acilis muzigi hakki hala var mi
bool acilisBasladi = false;      // acilis muzigi gercekten calmaya basladi mi
Uint64 acilisT0 = 0;             // acilis muzigi icin yeniden deneme penceresi baslangici

// hocadan kacarken git.mp3: kacis surerken calar, kacis bitince susar
void gitGuncelle(bool kac) {
#ifdef HAS_MIX
    if (!gitTrack) return;
    if (kac) {
        if (!MIX_TrackPlaying(gitTrack)) MIX_PlayTrack(gitTrack, 0);
    } else if (gitCaliyor) {
        MIX_StopTrack(gitTrack, 0);
    }
#endif
    gitCaliyor = kac;
}
void defterBaslat() {
#ifdef HAS_MIX
    if (defterTrack) MIX_PlayTrack(defterTrack, 0);
#endif
}
void defterDurdur() {
#ifdef HAS_MIX
    if (defterTrack) MIX_StopTrack(defterTrack, 0);
#endif
}
bool defterCaliyor() {
#ifdef HAS_MIX
    return defterTrack && MIX_TrackPlaying(defterTrack);
#else
    return false;
#endif
}
void defterSesiCal() { defterBaslat(); }
void rehberBaslat() {
#ifdef HAS_MIX
    if (rehberTrack) MIX_PlayTrack(rehberTrack, 0);
#endif
}
void rehberDurdur() {
#ifdef HAS_MIX
    if (rehberTrack) MIX_StopTrack(rehberTrack, 0);
#endif
}
void klasikBaslat() {
    klasikOynadi = false;
#ifdef HAS_MIX
    if (klasikTrack) MIX_PlayTrack(klasikTrack, 0);
#endif
}
void klasikDurdur() {
#ifdef HAS_MIX
    if (klasikTrack) MIX_StopTrack(klasikTrack, 0);
#endif
}
bool klasikCaliyor() {
#ifdef HAS_MIX
    return klasikTrack && MIX_TrackPlaying(klasikTrack);
#else
    return false;
#endif
}
// MUZIK YONETICISI: ayni anda tek muzik calar. Oncelik: klasik.mp3 > defter sarkisi.
//  - klasik.mp3 (acilis): oyun baslarken calar; akilli tahta acilinca veya kovalamaca baslayinca susar
//  - klasik.mp3 (uyanma): ilk yanlis cevapta, hoca uyanirken (kovaDurum 1)
//  - defter sarkisi: sadece akilli tahta acikken ve hoca henuz uyanmamisken
//  - tahta kapaninca, oyun bitince veya kovalamaca baslayinca defter sarkisi susar
void muzikGuncelle() {
    bool klasikIste = durum == 0 && kovaDurum == 1 && !digerMuzikSus();
    bool defterIste = durum == 0 && kovaDurum == 0 && tahta.acik;
    // acilis muzigi: tahta acik / kovalamaca / oyun bitti ise hakki biter
    if (acilisMuzik && (durum != 0 || kovaDurum != 0 || tahta.acik)) acilisMuzik = false;
    if (acilisMuzik) {
        if (klasikCaliyor()) acilisBasladi = true;
        else if (!acilisBasladi && SDL_GetTicks() - acilisT0 < 1500) klasikBaslat();   // ses cihazi gec hazir olursa tekrar dene
    }
    if (klasikIste) {
        defterDurdur();
        if (!klasikCaliyor() && kovaSure < 0.4) klasikBaslat();   // ilk anlarda calmadiysa tekrar dene
    } else if (klasikCaliyor() && !acilisMuzik) {
        klasikDurdur();
    }
    if (defterIste) { if (!defterCaliyor()) defterBaslat(); }
    else if (defterCaliyor()) defterDurdur();
}
double sonKapiSesi = -10;

void sesGonder(const std::vector<Sint16>& v) {
    if (sesAkis && !v.empty()) SDL_PutAudioStreamData(sesAkis, v.data(), (int)(v.size() * sizeof(Sint16)));
}
void tonEkle(std::vector<Sint16>& v, double hz, double sn, double gen) {
    int n = (int)(sn * 22050);
    for (int i = 0; i < n; i++) {
        double t = i / 22050.0, zarf = 1.0 - (double)i / n;
        double s = hz > 0 ? (std::sin(2 * PI * hz * t) > 0 ? 1.0 : -1.0) : 0.0;
        v.push_back((Sint16)(s * gen * (0.4 + 0.6 * zarf)));
    }
}
void kumandaCal() {
    std::vector<Sint16> v;
    for (int i = 0; i < 3; i++) {
        tonEkle(v, 1200, 0.10, 5000); tonEkle(v, 0, 0.04, 0);
        tonEkle(v, 1800, 0.10, 5000); tonEkle(v, 0, 0.06, 0);
    }
    sesGonder(v);
}
// Lsoda ve EBar kullanma sesleri (mp3 yoksa kisa yedek ses)
void sodaSesiCal() {
#ifdef HAS_MIX
    if (mikser && sodaMp3) { MIX_PlayAudio(mikser, sodaMp3); return; }
#endif
    std::vector<Sint16> v; tonEkle(v, 1200, 0.05, 4000); tonEkle(v, 0, 0.02, 0); tonEkle(v, 1500, 0.10, 4000); sesGonder(v);
}
void ebarSesiCal() {
#ifdef HAS_MIX
    if (mikser && ebarMp3) { MIX_PlayAudio(mikser, ebarMp3); return; }
#endif
    std::vector<Sint16> v; tonEkle(v, 600, 0.06, 4500); tonEkle(v, 900, 0.06, 4500); tonEkle(v, 1300, 0.10, 4500); sesGonder(v);
}
void dogruSesiCal() {                    // dogru cevap: dogru.mp3 (yoksa kisa yedek ses)
#ifdef HAS_MIX
    if (mikser && dogruMp3) { MIX_PlayAudio(mikser, dogruMp3); return; }
#endif
    std::vector<Sint16> v; tonEkle(v, 900, 0.07, 4000); tonEkle(v, 1300, 0.12, 4000); sesGonder(v);
}
void tehlikeSesiCal() {                  // sadece tehlike.mp3 yoksa: yedek alarm bipi
    std::vector<Sint16> v; tonEkle(v, 400, 0.12, 6000); tonEkle(v, 0, 0.04, 0); tonEkle(v, 400, 0.12, 6000); tonEkle(v, 0, 0.04, 0); tonEkle(v, 300, 0.20, 6000); sesGonder(v);
}
void ucakSesiCal() {
#ifdef HAS_MIX
    if (mikser && ucakMp3) { MIX_PlayAudio(mikser, ucakMp3); return; }
#endif
    std::vector<Sint16> v; tonEkle(v, 700, 0.04, 3500); tonEkle(v, 1000, 0.05, 3500); tonEkle(v, 1400, 0.08, 3500); sesGonder(v);
}
void kancaSesiCal() {                      // kanca.mp3: kanca atilinca (yoksa yedek bip)
#ifdef HAS_MIX
    if (mikser && kancaMp3) { MIX_PlayAudio(mikser, kancaMp3); return; }
#endif
    std::vector<Sint16> v; tonEkle(v, 500, 0.05, 4500); tonEkle(v, 900, 0.05, 4500); tonEkle(v, 1500, 0.08, 4500); sesGonder(v);
}
void otomatSesiCal() {
#ifdef HAS_MIX
    if (mikser && otomatMp3) { MIX_PlayAudio(mikser, otomatMp3); return; }
#endif
    std::vector<Sint16> v; tonEkle(v, 800, 0.05, 4000); tonEkle(v, 0, 0.03, 0); tonEkle(v, 500, 0.12, 4000); sesGonder(v);
}
void skusSesiCal() {
#ifdef HAS_MIX
    if (mikser && skusMp3) { MIX_PlayAudio(mikser, skusMp3); return; }
#endif
    std::vector<Sint16> v; tonEkle(v, 300, 0.08, 4000); tonEkle(v, 0, 0.03, 0); tonEkle(v, 220, 0.12, 4000); sesGonder(v);
}
void kapiSesiCal(double x, double y) {
    if (std::hypot(x - px, y - py) > 14) return;
    double t = SDL_GetTicks() / 1000.0;
    if (t - sonKapiSesi < 0.25) return;
    sonKapiSesi = t;
#ifdef HAS_MIX
    if (mikser && kapiMp3) { MIX_PlayAudio(mikser, kapiMp3); return; }
#endif
    std::vector<Sint16> v;
    int n = (int)(0.16 * 22050);
    for (int i = 0; i < n; i++) {   // mp3 yoksa yedek "gicirti"
        double z = 1.0 - (double)i / n;
        v.push_back((Sint16)(((std::rand() % 2000) - 1000) * 6 * z * z + std::sin(2 * PI * 110 * i / 22050.0) * 4000 * z));
    }
    sesGonder(v);
}

// gurultu: hoca daha hizli gelir (Yop varsa sessiz)
void gurultu() { if (yopSure > 0) return; hocaHizli = 6; }

// ================= HARITA =================
void haritaKur() {
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++) { harita[y][x] = T_DUVAR; koridor[y][x] = false; personelH[y][x] = false; }
    segler.clear(); sariKapilar.clear(); odalar.clear(); makOdalar.clear();
    auto ac = [&](int x0, int y0, int x1, int y1) {
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++) { harita[y][x] = T_BOS; koridor[y][x] = true; }
        segler.push_back({x0, y0, x1, y1, (x1 - x0) > (y1 - y0)});
    };
    auto sari = [&](int x, int y) { harita[y][x] = T_SARI; sariKapilar.push_back({x, y, false}); };

    // dallanan koridorlar (2 hucre genis)
    ac(1, 7, 45, 8);      // ust yatay
    ac(1, 16, 29, 17);    // orta yatay (sol ucu cikmaz)
    ac(13, 25, 45, 26);   // alt yatay (sag ucu cikmaz)
    ac(13, 1, 14, 26);    // sol dikey (ust ucu cikmaz)
    ac(28, 7, 29, 26);    // sag dikey
    // guney kanat: alt yatay koridorun sol basindan asagi inen baglanti + uzun guney koridor (6 makine odasi buraya acilir)
    ac(12, 27, 13, 33);   // baglanti (dikey)
    ac(1, 34, 45, 35);    // guney yatay koridor

    // 12 sinif
    odalar.push_back({1, 1, 6, 1, false});   odalar.push_back({7, 1, 6, 1, false});
    odalar.push_back({16, 1, 6, 1, false});  odalar.push_back({22, 1, 6, 1, false});
    odalar.push_back({31, 1, 6, 1, false});
    odalar.push_back({1, 10, 15, 10, false});  odalar.push_back({7, 10, 9, 14, false});
    odalar.push_back({16, 10, 15, 10, false}); odalar.push_back({22, 10, 9, 14, false});
    odalar.push_back({1, 19, 18, 23, false});  odalar.push_back({16, 19, 24, 19, false});
    odalar.push_back({22, 19, 24, 19, false});
    // 5 personel izinli oda
    odalar.push_back({7, 19, 18, 23, true});
    odalar.push_back({16, 28, 27, 32, true});  odalar.push_back({22, 28, 27, 32, true});
    odalar.push_back({28, 28, 27, 32, true});  odalar.push_back({34, 28, 27, 32, true});

    for (auto& o : odalar) {
        for (int y = o.y0; y <= o.y0 + 4; y++)
            for (int x = o.x0; x <= o.x0 + 4; x++) { harita[y][x] = T_BOS; if (o.personel) personelH[y][x] = true; }
        harita[o.kapiY][o.x0 + 2] = T_MAVI;   // mavi kapi
        if (o.personel) personelH[o.kapiY][o.x0 + 2] = true;
        int sr[2] = {o.y0 + 1, o.y0 + 3};
        int sk[4] = {0, 1, 3, 4};
        for (int a = 0; a < 2; a++)
            for (int b = 0; b < 4; b++) harita[sr[a]][o.x0 + sk[b]] = T_SIRA;   // siralar
    }

    // kafeterya (buyuk): x31-45, y10-23
    for (int y = 10; y <= 23; y++)
        for (int x = 31; x <= 45; x++) harita[y][x] = T_BOS;
    int tsat[4] = {11, 13, 19, 21};
    for (int a = 0; a < 4; a++)
        for (int x = 33; x <= 43; x++)
            if (x != 37 && x != 38) harita[tsat[a]][x] = T_SIRA;   // uzun masalar (ortada gecit)
    // kafeterya cikislari: sari kapilar
    sari(30, 16); sari(30, 17);
    sari(37, 9);  sari(38, 9);
    sari(37, 24); sari(38, 24);
    // koridor basi/sonu sari kapilar
    sari(1, 7);  sari(1, 8);
    sari(45, 7); sari(45, 8);
    sari(1, 16); sari(1, 17);
    sari(45, 25); sari(45, 26);
    sari(13, 1); sari(14, 1);
    sari(1, 34); sari(1, 35); sari(45, 34); sari(45, 35);   // guney koridorun basi/sonu
    // baslangic koridorunun sonu: iki defter cozulunce acilan kapi
    harita[16][7] = T_KILIT; harita[17][7] = T_KILIT;
    sariKapilar.push_back({7, 16, false, true});
    sariKapilar.push_back({7, 17, false, true});

    // ceza odasi: x38-44, y1-5 (kilitli kapi y6)
    for (int y = 1; y <= 5; y++)
        for (int x = 38; x <= 44; x++) harita[y][x] = T_BOS;
    harita[6][41] = T_KILIT;
    int cy2[2] = {2, 4}, cx2[4] = {39, 40, 42, 43};
    for (int a = 0; a < 2; a++) for (int b = 0; b < 4; b++) harita[cy2[a]][cx2[b]] = T_SIRA;
    int tb[7][2] = {{37, 2}, {37, 4}, {45, 2}, {45, 4}, {39, 0}, {41, 0}, {43, 0}};
    for (auto& t : tb) harita[t[1]][t[0]] = T_TABLO;

    // otomatlar (duvara gomulu)
    harita[12][46] = T_OTO_E; harita[20][46] = T_OTO_L;    // kafeterya
    harita[27][14] = T_OTO_E; harita[27][42] = T_OTO_L;    // koridorlar

    // makine odalari (toplam 7): 5x5, yesil kapi ust duvarda (x0+1, y0-1).
    // 1 tanesi eskisi gibi alt yatay koridorun altinda (x40,y28), 6 tanesi guney kanatta (y37).
    auto makOdaKur = [&](int x0, int y0) {
        for (int y = y0; y <= y0 + 4; y++)
            for (int x = x0; x <= x0 + 4; x++) harita[y][x] = T_BOS;
        harita[y0 - 1][x0 + 1] = T_MAVI; yesilH[y0 - 1][x0 + 1] = true;
        MakOda m; m.x0 = x0; m.y0 = y0; m.mx = x0 + 2.5; m.my = y0 + 0.75;
        m.bx0 = x0 + 0.5; m.bx1 = x0 + 4.5; m.by0 = y0 + 0.5; m.by1 = y0 + 4.5;
        makOdalar.push_back(m);
    };
    makOdaKur(40, 28);
    const int gx0[6] = {4, 11, 18, 25, 32, 39};
    for (int i = 0; i < 6; i++) makOdaKur(gx0[i], 37);

    std::memcpy(haritaOrj, harita, sizeof(harita));
}

const double CEZA_KX = 41.5, CEZA_KY = 3.5;   // ceza odasinda duracagin yer

void yeniden() {
    std::memcpy(harita, haritaOrj, sizeof(harita));
    for (auto& k : sariKapilar) { k.acik = false; k.kilitli = (k.x == 7 && (k.y == 16 || k.y == 17)); }
    kovaDurum = 0; kovaSure = 0; yanlisSayi = 0; cozulenDefter = 0; girisKilitli = true; quizAtla = false; tahta = Tahta();
    klasikDurdur(); defterDurdur();
    acilisMuzik = true; acilisBasladi = false; acilisT0 = SDL_GetTicks();   // oyun basinda klasik.mp3
    px = 3.5; py = 16.5; dx = 1; dy = 0; plx = 0; ply = 0.66;
    hx = 5.5; hy = 16.5; ux = 13.5; uy = 4.5;      // hoca ilk koridorda bekler
    mhx = 35.5; mhy = 7.5; mhKapi = 0; mhwx = -1; mhwy = -1;
    vlx = TEST_MOD ? 14.5 : 22.5; vly = TEST_MOD ? 16.5 : 7.5;   // test modunda kilitli kapinin hemen otesinde
    vKapi = 0; vwx = -1; vwy = -1; vDinlen = 0; veletOyalan = 0; kayH = kayU = kayM = kayV = Kay(); veletKos = false; veletTasi = false; vSeg = -1;
    vTasiAx = vTasiAy = vTasiKalan = 0;
    dumanlar.clear();
    ucak = Ucak();
    kpx = TEST_MOD ? 27.5 : 10.5; kpy = TEST_MOD ? 17.5 : 8.5;    // test modunda kilitli kapinin otesinde
    kpKapi = 0; kpwx = -1; kpwy = -1; kpDinlen = 0; kpKayip = 0; kpT = 0; kpWpT = 0; kpfx = -1; kpfy = -1;
    kpMod = 0; kpTip = -1; kpTehlikeCaldi = false; kpMuzik = false;
    msPuan = HILE ? 9999 : 0; elBalon = -1; balonlar.clear();
    for (int k = 0; k < (int)makOdalar.size(); k++) { makineSoruUret(k); balonlariUret(k); }   // HILE acikken baslangic MS 9999, kapaliyken 0
    kantinAcik = false; kantinMesajT = 0; kantinMesaj.clear();
    kanca = Kanca();
    nbx = TEST_MOD ? 22.5 : 30.5; nby = TEST_MOD ? 16.5 : 7.5;    // test modunda kilitli kapinin otesinde
    nbKapi = 0; nbwx = -1; nbwy = -1; nbDinlen = 0; nbKayip = 0; nbT = 0; nbBekle = 5.0;
    nbMod = 0; nbFaz = 0; nbHedef = 0; nbBasarisiz = 0; nbMesaj.clear();
    nbTepki = NOBET_TEPKI; nbUzgun = false; nbKacT = 0; nbEtiketT = 0; nbEkGor = false;
    ihbarSure = 0; ihbarTekrar = 0; ihbarX = 0; ihbarY = 0;
    toplanan = 0; durum = 0; sure = 0; cezaSayi = 0;
    hocaDonuk = 0; hocaHizli = 0; hKapi = 0;
    uAlarm = 0; uDonuk = 0; uKapi = 0; uwx = -1; uwy = -1;
    kosuEnerji = 100; yorgun = false; kosuyor = false;
    sonsuzSure = 0; isimlikSure = 0; yopSure = 0; kumandaSure = 0; cezaKalan = 0; bildirimSure = 0;
    kab = Kabarcik();
    for (int i = 0; i < SLOT; i++) { slot[i] = -1; adet[i] = 0; }
    sel = -1;
    slot[0] = E_KANCA; adet[0] = 1;                    // Kanca her zaman baslangicta cantada (diger hazir esyalar: HILE blogu)
    std::memset(dolu, 0, sizeof(dolu));

    defterler.clear(); esyalar.clear();
    std::vector<int> sinif;
    for (size_t i = 0; i < odalar.size(); i++) {
        Oda& o = odalar[i];
        if (o.personel) continue;
        int x = o.x0 + std::rand() % 5;
        defterler.push_back({x + 0.5, o.uzakSatir + 0.5, false});
        defterler.back().quiz = (o.x0 == 1 && (o.y0 == 10 || o.y0 == 19));   // baslangic koridorundaki 2 sinif
        dolu[o.uzakSatir][x] = true;
        sinif.push_back((int)i);
    }
    auto odaHucre = [&](const Oda& o, double& x, double& y) {
        for (int deneme = 0; deneme < 400; deneme++) {
            int cx = o.x0 + std::rand() % 5, cy = o.y0 + std::rand() % 5;
            if (harita[cy][cx] == T_BOS && !dolu[cy][cx]) { dolu[cy][cx] = true; x = cx + 0.5; y = cy + 0.5; return; }
        }
        x = o.x0 + 2.5; y = o.y0 + 2.5;
    };
    // lift: yerden yukseklik. Esya gozle ayni hizaya cikar ki nisangahla secilebilsin.
    auto esyaEkle = [&](double x, double y, int tip, double lift) {
        if (lift < 0.30) lift = std::max(0.30, 0.56 - esHS[tip]);
        esyalar.push_back({x, y, tip, false, lift});
    };
    for (int i = (int)sinif.size() - 1; i > 0; i--) std::swap(sinif[i], sinif[std::rand() % (i + 1)]);
    for (int k = 0; k < 4; k++) {          // 3 sinifta kumanda, 1 sinifta isimlik
        double x, y; odaHucre(odalar[sinif[k]], x, y);
        esyaEkle(x, y, k < 3 ? E_KUMANDA : E_ISIMLIK, 0.10);
    }
    for (int k = 0; k < 3 && !sinif.empty(); k++) {      // 3 sinifta 1'er Cetvel
        double x, y; odaHucre(odalar[sinif[(k * 2 + 1) % sinif.size()]], x, y);
        esyaEkle(x, y, E_CETVEL, 0.10);
    }
    for (int k = 0; k < 2 && !sinif.empty(); k++) {      // 2 sinifta 1'er Kagit Ucak
        double x, y; odaHucre(odalar[sinif[(k * 3 + 2) % sinif.size()]], x, y);
        esyaEkle(x, y, E_UCAK, 0.10);
    }
    for (auto& o : odalar) {               // personel odalari: 1 balyoz, 2 EBar, 3 Yop
        if (!o.personel) continue;
        int liste[7] = {E_BALYOZ, E_EBAR, E_EBAR, E_YOP, E_YOP, E_YOP, E_SILGI};   // + her personel odasinda 1 Silgi
        for (int k = 0; k < 7; k++) { double x, y; odaHucre(o, x, y); esyaEkle(x, y, liste[k], 0.10); }
    }
    // kafeterya masalari uzerinde 3 Lsoda
    std::vector<std::pair<int, int>> masaH;
    for (int y = 10; y <= 23; y++)
        for (int x = 31; x <= 45; x++) if (harita[y][x] == T_SIRA) masaH.push_back({x, y});
    for (int k = 0; k < 3 && !masaH.empty(); k++) {
        int i = std::rand() % (int)masaH.size();
        esyaEkle(masaH[i].first + 0.5, masaH[i].second + 0.5, E_LSODA, 0.40);
        masaH.erase(masaH.begin() + i);
    }
    // koridorlarda 5 kurus
    int adetK = 0;
    while (adetK < 5) {
        int x = std::rand() % MW, y = std::rand() % MH;
        if (!koridor[y][x] || harita[y][x] != T_BOS || dolu[y][x]) continue;
        if (std::hypot(x + 0.5 - px, y + 0.5 - py) < 4) continue;
        dolu[y][x] = true;
        esyaEkle(x + 0.5, y + 0.5, E_KURUS, 0.10);
        adetK++;
    }
    // ---- HILE (tek blok): true iken kantin esyalari cantada + her esyadan baslangic koridorundaki 2 sinifa dizilir ----
    if (HILE) {
        for (int i = 1; i < 5; i++) { slot[i] = KANTIN_URUN[i]; adet[i] = 1; }       // Diyet Lsoda, Muz, Naneli Seker, Kalem
        std::vector<int> bs;
        for (size_t i = 0; i < odalar.size(); i++)
            if (!odalar[i].personel && odalar[i].x0 == 1 && (odalar[i].y0 == 10 || odalar[i].y0 == 19)) bs.push_back((int)i);
        const int liste[2][10] = {{E_KUMANDA, E_KUMANDA, E_ISIMLIK, E_BALYOZ, E_KURUS, E_KURUS, E_KURUS, E_SILGI, E_CETVEL, -1},
                                 {E_EBAR, E_LSODA, E_LSODA, E_YOP, E_YOP, E_KURUS, E_KURUS, E_CETVEL, E_UCAK, -1}};
        for (size_t b = 0; b < bs.size() && b < 2; b++)
            for (int k = 0; k < 10 && liste[b][k] >= 0; k++) {
                double x, y; odaHucre(odalar[bs[b]], x, y);
                esyaEkle(x, y, liste[b][k], liste[b][k] == E_LSODA ? 0.40 : 0.10);
            }
    }
}

void menuOyna() { girisMenu = false; yeniden(); }

double rehberMaxKay() { return std::max(0.0, 15 * (double)REH_ROW - REH_LIST_H); }
void rehberKayKisitla() { rehberKay = std::clamp(rehberKay, 0.0, rehberMaxKay()); }
float rehberThumbH() { return std::max(40.f, REH_LIST_H * REH_LIST_H / (15 * REH_ROW)); }
float rehberThumbY() {
    double mk = rehberMaxKay();
    return REH_LIST_Y + (mk > 0 ? (float)(rehberKay / mk) : 0.f) * (REH_LIST_H - rehberThumbH());
}
void rehberThumbAyarla(float ly) {           // cubugun ortasi parmagin altinda olsun
    float aralik = REH_LIST_H - rehberThumbH();
    double t = aralik > 0 ? (ly - REH_LIST_Y - rehberThumbH() / 2) / aralik : 0.0;
    rehberKay = std::clamp(t, 0.0, 1.0) * rehberMaxKay();
}
void rehberAc() { rehberAcik = true; rehberKay = 0; rehberSurukle = 0; rehberBaslat(); }
void rehberKapat() { rehberAcik = false; rehberSurukle = 0; rehberDurdur(); }   // oyunu sifirdan baslatir (acilis muzigi de simdi baslar)

bool serbest(double x, double y) {
    const double r = 0.2;
    double xs[2] = {x - r, x + r}, ys[2] = {y - r, y + r};
    for (int a = 0; a < 2; a++)
        for (int b = 0; b < 2; b++) {
            int cx = (int)xs[a], cy = (int)ys[b];
            if (xs[a] < 0 || ys[b] < 0 || cx >= MW || cy >= MH) return false;
            int h = harita[cy][cx];
            if (h != T_BOS && h != T_MAVI_ACIK && h != T_SARI_ACIK) return false;
        }
    return true;
}

// iki nokta arasinda duvar / kapali kapi yoksa true (muhbirin gorusu)
bool gorusAcik(double x0, double y0, double x1, double y1) {
    double vx = x1 - x0, vy = y1 - y0, len = std::hypot(vx, vy);
    int n = std::max(1, (int)(len / 0.15));
    for (int i = 1; i < n; i++) {
        int cx = (int)(x0 + vx * i / n), cy = (int)(y0 + vy * i / n);
        if (cx < 0 || cy < 0 || cx >= MW || cy >= MH) return false;
        int h = harita[cy][cx];
        if (h != T_BOS && h != T_SIRA && h != T_MAVI_ACIK && h != T_SARI_ACIK) return false;
    }
    return true;
}

void bfsHesapla(int (*d)[MW], int sx, int sy) {
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++) d[y][x] = -1;
    if (sx < 0 || sy < 0 || sx >= MW || sy >= MH) return;
    std::queue<std::pair<int, int>> q;
    d[sy][sx] = 0;
    q.push({sx, sy});
    const int ox[4] = {1, -1, 0, 0}, oy[4] = {0, 0, 1, -1};
    while (!q.empty()) {
        auto [cx, cy] = q.front(); q.pop();
        for (int i = 0; i < 4; i++) {
            int nx = cx + ox[i], ny = cy + oy[i];
            if (nx < 0 || ny < 0 || nx >= MW || ny >= MH) continue;
            if (!gecilir(harita[ny][nx]) || d[ny][nx] != -1) continue;
            d[ny][nx] = d[cy][cx] + 1;
            q.push({nx, ny});
        }
    }
}

// kovalamaca basladigi an hoca, TUM haritadaki oyuncuya en uzak koridor noktasina isinlanir.
// Baslangic koridorunun kilitli kapisi (x=7) yol hesabinda acik sayilir; yoksa "en uzak" nokta hep
// oyuncunun yaninda kalirdi. Kapi kilitliyken hoca oyuncuya ulasamaz, kapi acilinca kovalamaya baslar.
void hocayiUzaklastir(bool zorla = false) {
    static int bfsU[MH][MW];
    for (int y = 0; y < MH; y++) for (int x = 0; x < MW; x++) bfsU[y][x] = -1;
    int sx = std::clamp((int)px, 0, MW - 1), sy = std::clamp((int)py, 0, MH - 1);
    std::queue<std::pair<int, int>> q;
    bfsU[sy][sx] = 0; q.push({sx, sy});
    const int ox[4] = {1, -1, 0, 0}, oy[4] = {0, 0, 1, -1};
    while (!q.empty()) {
        auto [cx, cy] = q.front(); q.pop();
        for (int i = 0; i < 4; i++) {
            int nx = cx + ox[i], ny = cy + oy[i];
            if (nx < 0 || ny < 0 || nx >= MW || ny >= MH || bfsU[ny][nx] != -1) continue;
            int h = harita[ny][nx];
            bool girisKapi = (h == T_KILIT && nx == 7 && (ny == 16 || ny == 17));
            if (!gecilir(h) && !girisKapi) continue;
            bfsU[ny][nx] = bfsU[cy][cx] + 1;
            q.push({nx, ny});
        }
    }
    int hcx = std::clamp((int)hx, 0, MW - 1), hcy = std::clamp((int)hy, 0, MH - 1);
    if (!zorla && bfsU[hcy][hcx] >= KOVA_UZAKLIK) return;   // zorla=false: hoca zaten uzaktaysa dokunma
    int enUzak = -1, euX = -1, euY = -1;
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++) {
            if (!koridor[y][x] || harita[y][x] != T_BOS || bfsU[y][x] < 0) continue;
            if (bfsU[y][x] > enUzak) { enUzak = bfsU[y][x]; euX = x; euY = y; }
        }
    if (euX >= 0) { hx = euX + 0.5; hy = euY + 0.5; hKapi = 0; }
}

// bir muhafizi (hoca / mudur) hedefe dogru yurut. d: hedeften BFS mesafe tablosu
void adimAt(double& x, double& y, double& kapiS, int (*d)[MW], double gx, double gy, double hiz, double dt, double gecik) {
    int cx = (int)x, cy = (int)y;
    if (cx < 0 || cy < 0 || cx >= MW || cy >= MH) return;
    double tx = gx, ty = gy;
    bool bekle = false;
    if (!(cx == (int)gx && cy == (int)gy)) {
        int best = d[cy][cx], bx = -1, by = -1;
        const int ox[4] = {1, -1, 0, 0}, oy[4] = {0, 0, 1, -1};
        if (best >= 0)
            for (int i = 0; i < 4; i++) {
                int nx = cx + ox[i], ny = cy + oy[i];
                if (nx < 0 || ny < 0 || nx >= MW || ny >= MH) continue;
                if (dumanIcinde(nx, ny) && !dumanIcinde(cx, cy)) continue;   // beyaz dumana girilmez
                if (d[ny][nx] >= 0 && d[ny][nx] < best) { best = d[ny][nx]; bx = nx; by = ny; }
            }
        if (bx >= 0) {
            tx = bx + 0.5; ty = by + 0.5;
            if (harita[by][bx] == T_MAVI && std::hypot(x - tx, y - ty) < 1.05) {
                kapiS += dt; bekle = true;            // kapali mavi kapiyi acmak zaman alir
                if (kapiS >= gecik) { harita[by][bx] = T_MAVI_ACIK; kapiSesiCal(tx, ty); kapiS = 0; bekle = false; }
            } else kapiS = 0;
        } else { tx = x; ty = y; }
    }
    if (!bekle) {
        double vx = tx - x, vy = ty - y, len = std::hypot(vx, vy), adim = hiz * dt;
        if (len > adim) { x += vx / len * adim; y += vy / len * adim; }
        else { x = tx; y = ty; }
    }
}

void don(double a) {
    double c = std::cos(a), s = std::sin(a);
    double od = dx; dx = dx * c - dy * s; dy = od * s + dy * c;
    double op = plx; plx = plx * c - ply * s; ply = op * s + ply * c;
}

// ================= ENVANTER / EYLEMLER =================
// Biriktirme YOK: her nesne (Kurus dahil) ayri bir slot kaplar. n tane eklemeye calisir, sigdigi kadarini ekler.
// Hepsi sigdiysa true doner.
bool envanterEkle(int tip, int n) {
    while (n > 0) {
        int idx = -1;
        for (int i = 0; i < SLOT; i++) if (slot[i] < 0) { idx = i; break; }
        if (idx < 0) return false;
        slot[idx] = tip; adet[idx] = 1; n--;
    }
    return true;
}
int kurusSayi() { int n = 0; for (int i = 0; i < SLOT; i++) if (slot[i] == E_KURUS) n++; return n; }
bool kurusHarca() {                   // 1 kurus slotunu bosaltir
    for (int i = 0; i < SLOT; i++) if (slot[i] == E_KURUS) { slot[i] = -1; adet[i] = 0; if (sel == i) sel = -1; return true; }
    return false;
}

// ---- akilli tahta ----
SDL_FRect tahtaSecRect(int i) { return {232.f + i * 178.f, 380.f, 168.f, 110.f}; }

void tahtaSoruKur() {
    Tahta& t = tahta;
    for (int i = 0; i < 3; i++) t.elendi[i] = false;
    t.pi = (t.defterNo == 1 && t.soru == 2);
    t.dogruIdx = std::rand() % 3;
    if (t.pi) {                                    // 2. defterin son sorusu: 2 + pi, TUM cevaplar yanlis
        t.metin = "2 + @ = ?";
        t.dogruIdx = -1;                           // 😈 hicbir sik dogru degil (5,14159265 yok)
        const char* c[3] = {"5,41592653", "5,14159256", "5,14195265"};
        for (int i = 0; i < 3; i++) t.sec[i] = c[i];
        return;
    }
    int tur = (t.defterNo == 0) ? t.soru : (t.soru == 0 ? 2 : 0);   // 0 topla, 1 cikar, 2 carp
    int a = 0, b = 0, sonuc = 0; char op = '+';
    if (tur == 0) {
        if (t.defterNo == 0) { a = 10 + std::rand() % 40; b = 10 + std::rand() % 30; }
        else { a = 120 + std::rand() % 360; b = 110 + std::rand() % 280; }
        sonuc = a + b; op = '+';
    } else if (tur == 1) {
        a = 40 + std::rand() % 60; b = 11 + std::rand() % 30; sonuc = a - b; op = '-';
    } else {
        if (t.defterNo == 0) { a = 2 + std::rand() % 8; b = 2 + std::rand() % 8; }
        else { a = 4 + std::rand() % 6; b = 6 + std::rand() % 4; }
        sonuc = a * b; op = 'x';
    }
    t.metin = std::to_string(a) + " " + op + " " + std::to_string(b) + " = ?";
    int fark[10] = {-10, -2, -1, 1, 2, 10, -3, 3, 5, -5};
    int yan[2], n = 0;
    while (n < 2) {
        int v = sonuc + fark[std::rand() % 10];
        if (v < 0 || v == sonuc || (n == 1 && v == yan[0])) continue;
        yan[n++] = v;
    }
    int j = 0;
    for (int i = 0; i < 3; i++) t.sec[i] = std::to_string(i == t.dogruIdx ? sonuc : yan[j++]);
}

void tahtaAc(int defterId) {
    tahta = Tahta();
    tahta.acik = true; tahta.defterId = defterId; tahta.defterNo = cozulenDefter; tahta.soru = 0;
    tahtaSoruKur();
    acilisMuzik = false; klasikDurdur();    // akilli tahta muzigi baslarken acilis muzigi susar
    defterSesiCal();
}

// baslangic koridorunun sonundaki kilitli kapiyi acar (2 defter bitince veya yanlis cevapta)
void girisKapisiniAc() {
    if (!girisKilitli) return;
    girisKilitli = false;
    harita[16][7] = T_SARI; harita[17][7] = T_SARI;
    for (auto& k : sariKapilar) if (k.x == 7 && (k.y == 16 || k.y == 17)) k.kilitli = false;
}

void tahtaCevap(int i) {
    if (!tahta.acik || tahta.atSure > 0 || i < 0 || i > 2 || tahta.elendi[i]) return;
    if (i == tahta.dogruIdx) {
        tahta.yesil[tahta.soru] = true;
        tahta.soru++;
        tahta.mesaj = "Dogru!";
        dogruSesiCal();
        if (tahta.soru >= 3) {                      // defter bitti
            defterler[tahta.defterId].alindi = true;
            toplanan++; cozulenDefter++;
            tahta.acik = false;
            if (cozulenDefter >= 2) {
                girisKapisiniAc();
                bildir("2 defter bitti! Koridorun sonundaki kapi acildi.");
            } else bildir("1. defter bitti! Simdi 2. defteri coz.");
            if (toplanan >= ODA_SAYISI) durum = 1;
        } else tahtaSoruKur();
    } else {
        tahta.elendi[i] = true;
        yanlisSayi++;
        quizAtla = true;                             // yanlis: artik butun defterler tahtasiz dogrudan toplanabilir
        if (girisKilitli) { girisKapisiniAc(); bildir("Yanlis! Kapi acildi, defterleri direkt topla, KAC!"); }   // yanlista kapi hemen acilir
        if (kovaDurum == 0) { kovaDurum = 1; kovaSure = 0; klasikBaslat(); tahta.mesaj = "Yanlis! Hoca uyaniyor..."; }
        else tahta.mesaj = "Yanlis! Hoca hizlandi!";
        tahta.atSure = TAHTA_AT_SURE;                // yanlis: kisa sure sonra tahtadan atilirsin
    }
}

void aksiyon() {
    if (durum != 0 || tahta.acik || nbMod == 2 || kantinAcik || kanca.var) return;
    if (hedefTip == 0 && hedefId >= 0 && defterler[hedefId].quiz && !defterler[hedefId].alindi && !quizAtla) {
        tahtaAc(hedefId);
    } else if (hedefTip == 0 && hedefId >= 0) {
        defterler[hedefId].alindi = true;
        toplanan++;
        if (toplanan >= ODA_SAYISI) durum = 1;
    } else if (hedefTip == 2 && hedefId >= 0) {
        Esya& e = esyalar[hedefId];
        if (envanterEkle(e.tip, 1)) { e.alindi = true; bildir(std::string("Aldin: ") + esAd[e.tip]); }
        else bildir("Canta dolu!");
    } else if (hedefTip == 7) {                                                        // kantinci: alisveris ekrani
        kantinAcik = true; kantinMesaj.clear(); kantinMesajT = 0;
    } else if (hedefTip == 5 && hedefId >= 0 && hedefId < (int)balonlar.size()) {      // balon al
        int yeni = balonlar[hedefId].deger;
        if (elBalon >= 0) balonlar[hedefId].deger = elBalon;                          // eldekini yerine birak (degis tokus)
        else balonlar.erase(balonlar.begin() + hedefId);
        elBalon = yeni;
        bildir("Balon alindi: " + std::to_string(elBalon));
    } else if (hedefTip == 6 && hedefId >= 0 && hedefId < (int)makOdalar.size()) {      // makineye balon ver
        const int k = hedefId;
        if (elBalon < 0) bildir("Makine: " + makOdalar[k].metin + "  Dogru sonuclu balonu getir!");
        else if (elBalon == makOdalar[k].cevap) {
            msPuan += MS_PUAN; elBalon = -1;
            makineSoruUret(k); balonlariUret(k);
            bildir("Dogru! +" + std::to_string(MS_PUAN) + " MS (Toplam: " + std::to_string(msPuan) + ")");
            dogruSesiCal();
        } else {
            bildir("Yanlis balon! Balon patladi.");
            elBalon = -1;
        }
    } else if (hedefTip == 3) {
        int& h = harita[hedefKY][hedefKX];
        double cx = hedefKX + 0.5, cy = hedefKY + 0.5;
        if (h == T_MAVI) { h = T_MAVI_ACIK; kapiSesiCal(cx, cy); gurultu(); }
        else if (h == T_MAVI_ACIK) {
            if (std::hypot(px - cx, py - cy) > 0.9 && std::hypot(hx - cx, hy - cy) > 0.9 && std::hypot(ux - cx, uy - cy) > 0.9 && std::hypot(mhx - cx, mhy - cy) > 0.9 && std::hypot(vlx - cx, vly - cy) > 0.9) h = T_MAVI;
        }
    } else if (hedefTip == 4) {
        int h = harita[hedefKY][hedefKX];
        int tip = (h == T_OTO_E) ? E_EBAR : E_LSODA;
        if (kurusSayi() <= 0) bildir("Kurus lazim!");
        else {
            kurusHarca();                                     // kurus slotu bosalir, esya o slota gelir
            envanterEkle(tip, 1);
            otomatSesiCal();
            bildir(std::string("Aldin: ") + esAd[tip]);
        }
    }
}

// Lsoda kabarcigi: koridorda, sinif/personel odasinda, kafeteryada ve makine odasinda kullanilir.
// Diyet Lsoda ayni seydir ama daha kisa gider: normal mesafenin DIYET_ORAN'i, en fazla DIYET_MESAFE karo.
const double DIYET_MESAFE = 6.0, DIYET_ORAN = 0.6;
const int KAF_X0 = 31, KAF_Y0 = 10, KAF_X1 = 45, KAF_Y1 = 23;    // kafeterya (haritaKur ile ayni)

// donus: 0 basladi, 1 burada kullanilamaz (alan yok), 2 onunde yeterli yer yok
int kabarcikBaslat(bool diyet) {
    int cx = (int)px, cy = (int)py;
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    bool oda = false, bulundu = false;
    for (auto& o : odalar)                                                  // sinif / personel odasi (5x5)
        if (cx >= o.x0 && cx <= o.x0 + 4 && cy >= o.y0 && cy <= o.y0 + 4) {
            x0 = o.x0; y0 = o.y0; x1 = o.x0 + 4; y1 = o.y0 + 4; oda = bulundu = true; break;
        }
    if (!bulundu && cx >= KAF_X0 && cx <= KAF_X1 && cy >= KAF_Y0 && cy <= KAF_Y1) {   // kafeterya
        x0 = KAF_X0; y0 = KAF_Y0; x1 = KAF_X1; y1 = KAF_Y1; oda = bulundu = true;
    }
    if (!bulundu)                                                           // makine odalari (7 adet)
        for (const auto& mo : makOdalar)
            if (cx >= mo.x0 && cx <= mo.x0 + 4 && cy >= mo.y0 && cy <= mo.y0 + 4) {
                x0 = mo.x0; y0 = mo.y0; x1 = mo.x0 + 4; y1 = mo.y0 + 4; oda = bulundu = true; break;
            }

    if (oda) {
        // oda / kafeterya: bakilan eksende (baskin yon) gider; masa, sira, duvar ve kapali kapiya kadar
        bool yatay = std::fabs(dx) >= std::fabs(dy);
        double ax = yatay ? (dx >= 0 ? 1 : -1) : 0, ay = yatay ? 0 : (dy >= 0 ? 1 : -1);
        auto uygun = [&](double x, double y) {
            return x >= x0 + 0.5 && x <= x1 + 0.5 && y >= y0 + 0.5 && y <= y1 + 0.5 && serbest(x, y);
        };
        double d0 = 0;                                                      // oyuncunun onunde en fazla 2 karo
        for (double d = 0.1; d <= 2.0001; d += 0.1) {
            if (!uygun(px + ax * d, py + ay * d)) break;
            d0 = d;
        }
        if (d0 < 1.4) return 2;                                             // 1.4'ten yakinsa kabarcik hemen seni de kapardi
        double d1 = d0;
        for (double d = d0 + 0.1; d <= 60.0; d += 0.1) {                    // engele kadar ne kadar gidebilir
            if (!uygun(px + ax * d, py + ay * d)) break;
            d1 = d;
        }
        kab = Kabarcik();
        kab.var = true; kab.diyet = diyet;
        kab.bx0 = x0; kab.by0 = y0; kab.bx1 = x1; kab.by1 = y1;
        kab.ax = ax; kab.ay = ay;
        kab.x = px + ax * d0; kab.y = py + ay * d0;
        kab.kalan = d1 - d0;
        if (diyet) kab.kalan = std::min(kab.kalan * DIYET_ORAN, DIYET_MESAFE);
        if (kab.kalan < 0.3) kab.kalan = 0.3;
        return 0;
    }

    // koridor (eski mantik): koridorun sonuna kadar
    int si = -1;
    double eniyi = -1;
    for (size_t i = 0; i < segler.size(); i++) {
        Seg& s = segler[i];
        if (cx < s.x0 || cx > s.x1 || cy < s.y0 || cy > s.y1) continue;
        double uyum = s.yatay ? std::fabs(dx) : std::fabs(dy);
        if (uyum > eniyi) { eniyi = uyum; si = (int)i; }
    }
    if (si < 0) return 1;
    Seg& s = segler[si];
    kab = Kabarcik();
    kab.var = true; kab.diyet = diyet;
    kab.bx0 = s.x0; kab.by0 = s.y0; kab.bx1 = s.x1; kab.by1 = s.y1;
    double ex;
    if (s.yatay) {
        int sg = dx >= 0 ? 1 : -1;
        kab.ax = sg; kab.ay = 0;
        kab.x = std::clamp(px + sg * 2.0, s.x0 + 0.9, s.x1 + 1 - 0.9);
        kab.y = (s.y0 + s.y1 + 1) / 2.0;
        ex = sg > 0 ? s.x1 + 1 - 0.9 : s.x0 + 0.9;
        kab.kalan = std::fabs(ex - kab.x);
    } else {
        int sg = dy >= 0 ? 1 : -1;
        kab.ax = 0; kab.ay = sg;
        kab.y = std::clamp(py + sg * 2.0, s.y0 + 0.9, s.y1 + 1 - 0.9);
        kab.x = (s.x0 + s.x1 + 1) / 2.0;
        ex = sg > 0 ? s.y1 + 1 - 0.9 : s.y0 + 0.9;
        kab.kalan = std::fabs(ex - kab.y);
    }
    if (diyet) kab.kalan = std::min(kab.kalan * DIYET_ORAN, DIYET_MESAFE);
    if (kab.kalan < 0.3) kab.kalan = 0.3;
    return 0;
}

void tuket() {
    adet[sel]--;
    if (adet[sel] <= 0) { slot[sel] = -1; adet[sel] = 0; sel = -1; }
}

void kullan() {
    if (durum != 0 || cezaKalan > 0 || nbMod == 2 || kantinAcik || kanca.var || sel < 0 || slot[sel] < 0) return;
    int t = slot[sel];
    if (t == E_KUMANDA) {
        kumandaCal();
        kumandaX = px; kumandaY = py; kumandaSure = 12; uAlarm = 0;
        bildir("Kumanda calisti! Mudur geliyor...");
        tuket();
    } else if (t == E_ISIMLIK) {
        isimlikSure = 30; bildir("Isimlik takildi: 30sn Mudur'den korunursun");
        tuket();
    } else if (t == E_BALYOZ) {
        if (hedefTip == 4) {
            int& h = harita[hedefKY][hedefKX];
            int tip = (h == T_OTO_E) ? E_EBAR : E_LSODA;
            h = T_OTO_KIRIK;
            gurultu();
            tuket();                                          // balyoz slotu bosalir, 3 esya icin yer acilir
            bool tamam = envanterEkle(tip, 3);
            bildir(tamam ? std::string("Otomat parcalandi: 3 ") + esAd[tip] : std::string("Otomat parcalandi ama canta doldu, bazilari dustu!"));
        } else bildir("Balyoz icin bir otomata bak!");
    } else if (t == E_EBAR) {
        kosuEnerji = 100; sonsuzSure = 10; yorgun = false;
        bildir("EBar: 10sn sinirsiz kosu!");
        ebarSesiCal();
        tuket();
    } else if (t == E_LSODA || t == E_DIYET) {
        bool diyet = (t == E_DIYET);
        int kr = kabarcikBaslat(diyet);
        if (kr == 0) { bildir(std::string(diyet ? "Diyet Lsoda: kisa kabarcik yayildi!" : "Lsoda: kabarcik yayildi!")); sodaSesiCal(); tuket(); }
        else if (kr == 1) bildir("Lsoda'yi oda, kafeterya veya koridorda kullan!");
        else bildir("Lsoda: onunde yeterli yer yok!");
    } else if (t == E_YOP) {
        yopSure = 30; bildir("Yop: 30sn kapi ve otomat sessiz");
        skusSesiCal();
        tuket();
    } else if (t == E_UCAK) {
        if (kovaDurum != 2) { bildir("Kagit ucak: hoca kovalarken at!"); return; }
        if (ucak.var) { bildir("Ucak zaten havada!"); return; }
        double l = std::hypot(dx, dy); if (l < 0.001) l = 1;
        ucak.var = true; ucak.ax = dx / l; ucak.ay = dy / l;
        ucak.x = px + ucak.ax * 0.5; ucak.y = py + ucak.ay * 0.5; ucak.kalan = UCAK_MESAFE;
        ucakSesiCal();
        bildir("Kagit ucak firlatildi!");
        tuket();
    } else if (t == E_KANCA) {
        if (kanca.var) return;
        double l = std::hypot(dx, dy); if (l < 0.001) l = 1;
        double ax = dx / l, ay = dy / l, bx = px, by = py;
        for (double d = 0.15; d <= KANCA_MENZIL; d += 0.1) {                // duvara / kapiya / siraya kadar git
            double nx = px + ax * d, ny = py + ay * d;
            if (!serbest(nx, ny)) break;
            bx = nx; by = ny;
        }
        if (std::hypot(bx - px, by - py) < 1.0) { bildir("Kanca: onunde yeterli yer yok!"); return; }
        kanca.var = true; kanca.tx = bx; kanca.ty = by;
        kancaSesiCal();
        bildir("Kanca firlatildi!");
        tuket();
    } else if (t == E_SEKER) {
        double uz = std::hypot(vlx - px, vly - py);
        if (!veletTasi && (uz > VELET_GORUS || !gorusAcik(vlx, vly, px, py))) { bildir("Naneli Seker: Veleti gorunce kullan!"); return; }
        if (veletOyalan > 0) { bildir("Velet zaten sekerle oyalaniyor!"); return; }
        veletTasi = false; veletKos = false; veletOyalan = SEKER_SURE;     // seni tasiyorsa birakir
        std::vector<Sint16> v; tonEkle(v, 1100, 0.04, 4500); tonEkle(v, 0, 0.02, 0); tonEkle(v, 1500, 0.06, 4500); sesGonder(v);
        bildir("Velet nane sekerine daldi: 5sn oyalaniyor!");
        tuket();
    } else if (t == E_MUZ) {
        double l = std::hypot(dx, dy); if (l < 0.001) l = 1;
        double ax = dx / l, ay = dy / l, bx = px, by = py;
        for (double d = 0.1; d <= MUZ_MENZIL; d += 0.1) {                   // duvara / kapiya / siraya kadar
            double nx = px + ax * d, ny = py + ay * d;
            if (!serbest(nx, ny)) break;
            bx = nx; by = ny;
        }
        if (std::hypot(bx - px, by - py) < 0.8) { bildir("Muz kabugu: onunde yeterli yer yok!"); return; }
        esyalar.push_back({bx, by, E_MUZ, false, 0.10});                    // yerdeki esya: dusman basinca kayar; sen de geri alabilirsin
        std::vector<Sint16> v; tonEkle(v, 700, 0.04, 4000); tonEkle(v, 500, 0.05, 4000); sesGonder(v);
        bildir("Muz kabugu yere atildi!");
        tuket();
    } else if (t == E_KALEM) {
        bildir("Kalem: sec ve Nobetci yanina gelince kendisi kullanilir");           // elle harcanmaz
    } else if (t == E_KURUS) {
        bildir("Kurus: otomata bakip aksiyon tusuna bas");
    } else if (t == E_CETVEL) {
        int bi = -1; double bd = 2.4;                                  // en yakin, bakilan yondeki sari kapi
        for (size_t i = 0; i < sariKapilar.size(); i++) {
            const SariKapi& k = sariKapilar[i];
            if (k.kilitli || k.sikis > 0) continue;
            double cx = k.x + 0.5, cy = k.y + 0.5, d = std::hypot(cx - px, cy - py);
            if (d >= bd) continue;
            double dot = d > 0.01 ? ((cx - px) * dx + (cy - py) * dy) / d : 1.0;
            if (d > 1.0 && dot < 0.3) continue;
            bi = (int)i; bd = d;
        }
        if (bi < 0) { bildir("Cetvel icin sari kapiya yaklas ve ona bak!"); return; }
        std::vector<int> cift = {bi};                                   // kapinin yanindaki es kanat da sikisir
        for (size_t i = 0; i < sariKapilar.size(); i++) {
            const SariKapi& k = sariKapilar[i];
            const SariKapi& k0 = sariKapilar[bi];
            if ((int)i != bi && !k.kilitli && k.sikis <= 0 && std::abs(k.x - k0.x) + std::abs(k.y - k0.y) == 1) cift.push_back((int)i);
        }
        for (int i : cift) {                                            // kapida biri varsa sikistirma
            double cx = sariKapilar[i].x + 0.5, cy = sariKapilar[i].y + 0.5;
            if (std::hypot(px - cx, py - cy) < 0.9 || std::hypot(hx - cx, hy - cy) < 1.0 || std::hypot(ux - cx, uy - cy) < 1.0 ||
                std::hypot(mhx - cx, mhy - cy) < 1.0 || std::hypot(vlx - cx, vly - cy) < 1.0 || std::hypot(nbx - cx, nby - cy) < 1.0 || std::hypot(kpx - cx, kpy - cy) < 1.0) {
                bildir("Kapida biri var, biraz uzaklas!"); return;
            }
        }
        for (int i : cift) {
            SariKapi& k = sariKapilar[i];
            k.sikis = CETVEL_SURE; k.kilitli = true; k.acik = false; harita[k.y][k.x] = T_CETVEL;
        }
        bildir("Cetvel kapiya sikisti! 15sn kullanilamaz");
        tuket();
    } else if (t == E_SILGI) {
        double cx = px + dx * 2.2, cy = py + dy * 2.2;                  // bakilan yone, 2.2 karo ileriye
        if (cx < 1 || cy < 1 || cx >= MW - 1 || cy >= MH - 1 || !gecilir(harita[(int)cy][(int)cx])) { cx = px; cy = py; }
        dumanlar.push_back({cx, cy, DUMAN_SURE});
        bildir("Silgi: beyaz duman yayildi! 10sn kimse giremez");
        tuket();
    }
}

// Mudur yakaladi: ceza odasina at
void mudurYakala() {
    cezaSayi++;
    cezaKalan = 15.0 * cezaSayi;
    px = CEZA_KX; py = CEZA_KY; dx = 1; dy = 0; plx = 0; ply = 0.66;
    don(PI / 2);
    harita[6][41] = T_KILIT;
    ux = 13.5; uy = 4.5; uAlarm = 0; uDonuk = 0; kumandaSure = 0; uwx = -1; uwy = -1;
    kab = Kabarcik();
    bildir("Mudur yakaladi! Ceza: " + std::to_string((int)cezaKalan) + " sn");
}

// ---- nobetci ogrenci: refleks testi ----
SDL_FRect nbKagitRect() {                  // kagit once asagidadir, sonra yukari kalkar (kutulara basilabilsin diye)
    const float W = 340, H = 400, yUst = 60;
    float y = yUst;
    if (nbFaz == 0) { double k = std::clamp(nbT / 0.7, 0.0, 1.0); k = 1 - (1 - k) * (1 - k); y = (float)(SH - (SH - yUst) * k); }
    return {(SW - W) / 2, y, W, H};
}
SDL_FRect nbDugme(int i) {                 // 0: yesil (sol alt), 1: mavi (sag alt)
    SDL_FRect p = nbKagitRect();
    return {i == 0 ? p.x + 16 : p.x + p.w - 112, p.y + p.h - 100, 96, 84};
}
void kantinAl(int i) {                      // i: 0..4
    if (i < 0 || i >= 5) return;
    int tip = KANTIN_URUN[i], fiyat = KANTIN_FIYAT[i];
    bool yer = false; for (int k = 0; k < SLOT; k++) if (slot[k] < 0) yer = true;
    if (msPuan < fiyat) kantinMesaj = "Yetersiz MS! (" + std::to_string(msPuan) + "/" + std::to_string(fiyat) + ")";
    else if (!yer) kantinMesaj = "Canta dolu!";
    else {
        msPuan -= fiyat; envanterEkle(tip, 1);
        kantinMesaj = std::string("Aldin: ") + esAd[tip] + " (-" + std::to_string(fiyat) + " MS)";
        std::vector<Sint16> v; tonEkle(v, 700, 0.05, 4000); tonEkle(v, 1000, 0.08, 4000); sesGonder(v);
    }
    kantinMesajT = 2.5;
}
void nobetBaslatTur() {
    nbFaz = 0; nbT = 0; nbMesaj.clear();
    nbBekle = NOBET_BEKLE_MIN + (NOBET_BEKLE_MAX - NOBET_BEKLE_MIN) * (std::rand() % 1001) / 1000.0;
    nbHedef = std::rand() % 2;
}
void nobetBitir(const std::string& m) {
    nbMod = 0; nbFaz = 0; nbDinlen = NOBET_DINLEN; nbwx = -1; nbMesaj.clear();
    bildir(m);
}
void nobetBasarisiz(const std::string& neden) {
    nbBasarisiz++;
    if (nbBasarisiz >= NOBET_HAK) { nobetBitir("Nobetci 5. basarisizliktan sonra gitti!"); return; }
    nbFaz = 3; nbT = 0;
    nbMesaj = neden + " (" + std::to_string(nbBasarisiz) + "/" + std::to_string(NOBET_HAK) + ") Tekrar!";
}
void nobetTikla(int renk) {                // renk: 0 yesil, 1 mavi
    if (nbMod != 2) return;
    if (nbFaz == 0 || nbFaz == 3) return;  // kagit kalkarken / sonuc mesajinda basma sayilmaz
    if (nbFaz == 1) { nobetBasarisiz("Erken bastin!"); return; }
    if (renk == nbHedef) nobetBitir("Refleks testi gecildi! Nobetci gitti.");
    else nobetBasarisiz("Yanlis renk!");
}
void nobetGuncelle(double dt, bool donmus) {
    if (nbDinlen > 0) nbDinlen -= dt;
    if (nbEtiketT > 0) nbEtiketT -= dt;
    if ((durum != 0 || donmus || cezaKalan > 0) && nbMod != 0) { nbMod = 0; nbFaz = 0; nbDinlen = NOBET_DINLEN; nbwx = -1; }
    if (donmus || durum != 0) return;
    if (nbMod == 3) {                      // kalem: kagit bozuldu, nobetci uzgun ve hizli uzaklasir
        nbKacT -= dt;
        bfsHesapla(bfsN, std::clamp((int)nbKacX, 0, MW - 1), std::clamp((int)nbKacY, 0, MH - 1));
        adimAt(nbx, nby, nbKapi, bfsN, nbKacX, nbKacY, NOBET_HIZ_KAC, dt, 1.0);
        if (nbKacT <= 0 || std::hypot(nbx - nbKacX, nby - nbKacY) < 0.8) { nbMod = 0; nbDinlen = NOBET_DINLEN; nbwx = -1; }
        return;
    }
    if (nbMod == 2) {                      // refleks testi (oyuncu hareketsiz)
        nbT += dt;
        if (nbFaz == 0 && nbT >= 0.7) { nbFaz = 1; nbT = 0; }
        else if (nbFaz == 1 && nbT >= nbBekle) { nbFaz = 2; nbT = 0; }
        else if (nbFaz == 2 && nbT >= nbTepki) nobetBasarisiz("Gec kaldin!");
        else if (nbFaz == 3 && nbT >= 0.9) nobetBaslatTur();
        return;
    }
    bool gor = nbDinlen <= 0 && !tahta.acik && !kantinAcik && std::hypot(nbx - px, nby - py) < NOBET_GORUS && gorusAcik(nbx, nby, px, py);
    if (gor) { nbMod = 1; nbKayip = 0; }
    else if (nbMod == 1) { nbKayip += dt; if (nbKayip > 2.0) nbMod = 0; }
    if (nbMod == 1) {                      // yaklas
        bfsHesapla(bfsN, std::clamp((int)px, 0, MW - 1), std::clamp((int)py, 0, MH - 1));
        adimAt(nbx, nby, nbKapi, bfsN, px, py, NOBET_HIZ_GEL, dt, 1.5);
        if (std::hypot(nbx - px, nby - py) < 0.9) {
            if (sel >= 0 && slot[sel] == E_KALEM) {
                // SECILI kalem: kagit bozulur, test yapilmaz; nobetci uzgun uzaklasir. Bedel: testler artik 0.7 sn
                tuket();
                nbUzgun = true; nbTepki = NOBET_TEPKI_KALEM;
                nbMod = 3; nbKacT = NOBET_KAC_SURE; nbEtiketT = NOBET_ETIKET_SURE;
                int en = -1; nbKacX = nbx; nbKacY = nby;                         // senden en uzak koridor noktasi
                for (int y = 0; y < MH; y++) for (int x = 0; x < MW; x++)
                    if (koridor[y][x] && harita[y][x] == T_BOS && bfsN[y][x] > en && !(girisKilitli && x <= 6 && y >= 16 && y <= 17)) { en = bfsN[y][x]; nbKacX = x + 0.5; nbKacY = y + 0.5; }
                std::vector<Sint16> v; tonEkle(v, 900, 0.08, 4500); tonEkle(v, 600, 0.10, 4500); tonEkle(v, 350, 0.18, 4500); sesGonder(v);
                bildir("Kalem kagidi bozdu! Ama artik refleks testi 0.7 sn");
            } else {
                nbMod = 2; nbBasarisiz = 0; nobetBaslatTur();
                bildir("Nobetci Ogrenci: refleks testi! Adi yazan renge hemen bas");
            }
        }
    } else {                               // gezin
        if (nbwx < 0 || std::hypot(nbx - nbwx, nby - nbwy) < 0.8) {
            for (int deneme = 0; deneme < 500; deneme++) {
                int x = std::rand() % MW, y = std::rand() % MH;
                if (koridor[y][x] && harita[y][x] == T_BOS && !(girisKilitli && x <= 6 && y >= 16 && y <= 17)) { nbwx = x + 0.5; nbwy = y + 0.5; break; }
            }
        }
        if (nbwx >= 0) {
            bfsHesapla(bfsN, (int)nbwx, (int)nbwy);
            adimAt(nbx, nby, nbKapi, bfsN, nbwx, nbwy, NOBET_HIZ_GEZ, dt, 1.5);
        }
    }
}
// ---- kopyaci cocuk ----
void esyaYereBirak(double x, double y, int tip) {          // esyayi yere birakir (yeniden()'deki esyaEkle ile ayni mantik)
    double lift = (tip == E_LSODA) ? 0.40 : 0.10;
    if (lift < 0.30) lift = std::max(0.30, 0.56 - esHS[tip]);
    esyalar.push_back({x, y, tip, false, lift});
}
void kopyaciGuncelle(double dt, bool donmus) {
    if (kpDinlen > 0) kpDinlen -= dt;
    bool muzikOnceki = kpMuzik;
    kpMuzik = false;                                           // her kare sifirla; sadece yaklasirken ve yakindayken true olur
    if (durum != 0) return;
    bool mesgul = donmus || cezaKalan > 0 || nbMod == 2 || tahta.acik || kantinAcik;
    int doluSlot = 0; for (int i = 0; i < SLOT; i++) if (slot[i] >= 0) doluSlot++;
    double uzak = std::hypot(kpx - px, kpy - py);

    if (kpMod == 2) {                                          // esyayla kaciyor
        kpT += dt; kpWpT -= dt;
        if (kpWpT <= 0 || kpfx < 0 || std::hypot(kpx - kpfx, kpy - kpfy) < 0.8) {   // senden en uzak koridor noktalarindan birini sec
            int en = -1;
            for (int y = 0; y < MH; y++) for (int x = 0; x < MW; x++)
                if (koridor[y][x] && harita[y][x] == T_BOS && bfsP[y][x] > en && !(girisKilitli && x <= 6 && y >= 16 && y <= 17)) en = bfsP[y][x];
            for (int deneme = 0; deneme < 600 && en >= 0; deneme++) {
                int x = std::rand() % MW, y = std::rand() % MH;
                if (koridor[y][x] && harita[y][x] == T_BOS && bfsP[y][x] >= en - 4 && !(girisKilitli && x <= 6 && y >= 16 && y <= 17)) { kpfx = x + 0.5; kpfy = y + 0.5; break; }
            }
            kpWpT = 1.5;
        }
        if (kpfx >= 0) {
            bfsHesapla(bfsK, (int)kpfx, (int)kpfy);
            adimAt(kpx, kpy, kpKapi, bfsK, kpfx, kpfy, KP_HIZ_KAC, dt, 1.0);
        }
        if (!mesgul && uzak < 0.8) {                           // yakaladin: esyani geri al
            if (envanterEkle(kpTip, 1)) bildir(std::string("Esyani geri aldin: ") + esAd[kpTip]);
            else { esyaYereBirak(kpx, kpy, kpTip); bildir("Canta dolu! Esya yere dustu."); }
            kpTip = -1; kpMod = 0; kpDinlen = KP_DINLEN; kpwx = -1;
        } else if (kpT > KP_KACIS_SURE) {                      // kacti: esyayi bir yere birakir
            esyaYereBirak(kpx, kpy, kpTip);
            bildir(std::string("Kopyaci kacti! ") + esAd[kpTip] + " bir yere dustu.");
            kpTip = -1; kpMod = 0; kpDinlen = KP_DINLEN; kpwx = -1;
        }
        return;
    }

    bool gor = !mesgul && kpDinlen <= 0 && doluSlot > 0 && uzak < KP_GORUS && gorusAcik(kpx, kpy, px, py);
    if (gor) { if (kpMod != 1) kpTehlikeCaldi = false; kpMod = 1; kpKayip = 0; }
    else if (kpMod == 1) { kpKayip += dt; if (kpKayip > 2.0 || mesgul || doluSlot == 0) kpMod = 0; }

    if (kpMod == 1) {                                          // yaklas
        kpMuzik = uzak < (muzikOnceki ? KP_TEHLIKE_BIRAK : KP_TEHLIKE_MESAFE);   // yakinken calar, biraz uzaklasinca biter
        if (!kpTehlikeCaldi && uzak < KP_TEHLIKE_MESAFE) {
            kpTehlikeCaldi = true;
            bildir("Kopyaci Cocuk geliyor! Esyalarini koru!");
        }
        bfsHesapla(bfsK, std::clamp((int)px, 0, MW - 1), std::clamp((int)py, 0, MH - 1));
        adimAt(kpx, kpy, kpKapi, bfsK, px, py, KP_HIZ_GEL, dt, 1.5);
        if (!mesgul && doluSlot > 0 && std::hypot(kpx - px, kpy - py) < 0.7) {     // calar
            int adaylar[SLOT], n = 0;
            for (int i = 0; i < SLOT; i++) if (slot[i] >= 0) adaylar[n++] = i;
            int i = adaylar[std::rand() % n];
            kpTip = slot[i]; slot[i] = -1; adet[i] = 0; if (sel == i) sel = -1;
            kpMod = 2; kpT = 0; kpWpT = 0; kpfx = -1;
            bildir(std::string("Kopyaci '") + esAd[kpTip] + "' esyani caldi! Yakala!");
        }
    } else {                                                   // gezin
        if (kpwx < 0 || std::hypot(kpx - kpwx, kpy - kpwy) < 0.8) {
            for (int deneme = 0; deneme < 500; deneme++) {
                int x = std::rand() % MW, y = std::rand() % MH;
                if (koridor[y][x] && harita[y][x] == T_BOS && !(girisKilitli && x <= 6 && y >= 16 && y <= 17)) { kpwx = x + 0.5; kpwy = y + 0.5; break; }
            }
        }
        if (kpwx >= 0) {
            bfsHesapla(bfsK, (int)kpwx, (int)kpwy);
            adimAt(kpx, kpy, kpKapi, bfsK, kpwx, kpwy, KP_HIZ_GEZ, dt, 1.5);
        }
    }
}
void tehlikeGuncelle() {                   // tehlike.mp3: kopyaci yakinken calar, uzaklasinca / calinca / kaybolunca hemen susar
    static bool onceki = false;
#ifdef HAS_MIX
    if (tehlikeTrack) {
        if (kpMuzik) { if (!MIX_TrackPlaying(tehlikeTrack)) MIX_PlayTrack(tehlikeTrack, 0); }
        else if (MIX_TrackPlaying(tehlikeTrack)) MIX_StopTrack(tehlikeTrack, 0);
        onceki = kpMuzik;
        return;
    }
#endif
    if (kpMuzik && !onceki) tehlikeSesiCal();                  // mp3 yoksa yedek bip
    onceki = kpMuzik;
}
void dukkanGuncelle() {                    // dukkan.mp3: kantin acikken calar (dunya donmus), kapaninca susar
#ifdef HAS_MIX
    if (!dukkanTrack) return;
    if (kantinAcik) { if (!MIX_TrackPlaying(dukkanTrack)) MIX_PlayTrack(dukkanTrack, 0); }
    else if (MIX_TrackPlaying(dukkanTrack)) MIX_StopTrack(dukkanTrack, 0);
#endif
}
void vatGuncelle() {                       // vat.mp3: nobetci gelirken calar, bitince susar
#ifdef HAS_MIX
    if (!vatTrack) return;
    if (nobetMuzik()) { if (!MIX_TrackPlaying(vatTrack)) MIX_PlayTrack(vatTrack, 0); }
    else if (MIX_TrackPlaying(vatTrack)) MIX_StopTrack(vatTrack, 0);
#endif
}

void sariKapiGuncelle() {
    for (auto& k : sariKapilar) {
        if (k.kilitli) continue;
        double cx = k.x + 0.5, cy = k.y + 0.5;
        double R = k.acik ? 2.0 : 1.5;
        bool yakin = std::hypot(px - cx, py - cy) < R || std::hypot(hx - cx, hy - cy) < R ||
                     std::hypot(ux - cx, uy - cy) < R || std::hypot(mhx - cx, mhy - cy) < R || std::hypot(vlx - cx, vly - cy) < R || std::hypot(nbx - cx, nby - cy) < R || std::hypot(kpx - cx, kpy - cy) < R ||
                     (kab.var && std::hypot(kab.x - cx, kab.y - cy) < 2.2);
        if (yakin && !k.acik) {
            k.acik = true; harita[k.y][k.x] = T_SARI_ACIK;
            kapiSesiCal(cx, cy);
            if (std::hypot(px - cx, py - cy) < 1.5) gurultu();
        } else if (!yakin && k.acik) {
            k.acik = false; harita[k.y][k.x] = T_SARI;
        }
    }
}

// ================= DUVAR DOKULARI =================
static void tabanDuvar(double wx, double v, int& R, int& G, int& B) {   // krem duvar + mavi alt bant
    if (v < 0.69) { R = 235; G = 220; B = 160; }
    else if (v < 0.71) { R = 120; G = 110; B = 80; }
    else { R = 60; G = 90; B = 150; }
    if (wx < 0.02) { R -= 25; G -= 25; B -= 25; }
}

static void duvarRenk(int tip, double wx, double v, int cx, int cy, int& R, int& G, int& B) {
    if (tip == T_MAVI) {                   // mavi kapi
        R = 40; G = 95; B = 215;
        bool cerceve = wx < 0.08 || wx > 0.92 || v < 0.06;
        if (cerceve) { R = 20; G = 45; B = 120; }
        else if (wx > 0.18 && wx < 0.82 && ((v > 0.12 && v < 0.45) || (v > 0.55 && v < 0.92))) { R = 55; G = 115; B = 235; }
        if (yesilH[cy][cx]) std::swap(G, B);                                // makine odasinin kapisi: yesil
        if (wx > 0.78 && wx < 0.86 && v > 0.5 && v < 0.58) { R = 255; G = 215; B = 60; }
    } else if (tip == T_SARI) {            // sari sallanan kapi
        R = 238; G = 196; B = 40;
        bool cerceve = wx < 0.07 || wx > 0.93 || v < 0.05;
        if (cerceve) { R = 150; G = 110; B = 15; }
        else if (v > 0.12 && v < 0.40 && wx > 0.2 && wx < 0.8) { R = 255; G = 240; B = 160; }
        else if (v > 0.55 && v < 0.60) { R = 200; G = 150; B = 20; }
        if (wx > 0.74 && wx < 0.84 && v > 0.62 && v < 0.70) { R = 90; G = 60; B = 10; }
    } else if (tip == T_TABLO) {           // duvarda tablo
        tabanDuvar(wx, v, R, G, B);
        if (wx > 0.16 && wx < 0.84 && v > 0.10 && v < 0.60) {
            R = 110; G = 70; B = 30;
            if (wx > 0.21 && wx < 0.79 && v > 0.15 && v < 0.55) {
                double pu = (wx - 0.21) / 0.58, pv = (v - 0.15) / 0.40;
                int desen = (cx * 7 + cy * 3) % 3;
                if (desen == 0) {
                    if (pv < 0.58) { R = 120; G = 180; B = 235; } else { R = 70; G = 150; B = 80; }
                    if (std::hypot(pu - 0.75, pv - 0.25) < 0.14) { R = 255; G = 220; B = 60; }
                } else if (desen == 1) {
                    if (pu < 0.33) { R = 200; G = 60; B = 60; } else if (pu < 0.66) { R = 235; G = 200; B = 60; } else { R = 60; G = 90; B = 200; }
                    if (std::hypot(pu - 0.5, pv - 0.5) < 0.2) { R = 240; G = 240; B = 240; }
                } else {
                    R = 90; G = 70; B = 110;
                    double ex = (pu - 0.5) / 0.22, ey = (pv - 0.5) / 0.34;
                    if (ex * ex + ey * ey < 1.0) {
                        R = 235; G = 190; B = 150;
                        if (pv < 0.33) { R = 60; G = 40; B = 30; }
                        if (pv > 0.42 && pv < 0.5 && (std::fabs(pu - 0.42) < 0.04 || std::fabs(pu - 0.58) < 0.04)) { R = 20; G = 20; B = 20; }
                    }
                }
            }
        }
    } else if (tip == T_OTO_E || tip == T_OTO_L) {   // otomat
        int it = (tip == T_OTO_E) ? E_EBAR : E_LSODA;
        if (tip == T_OTO_E) { R = 130; G = 70; B = 60; } else { R = 50; G = 70; B = 170; }
        if (wx < 0.06 || wx > 0.94 || v < 0.04) { R /= 2; G /= 2; B /= 2; }
        else if (wx > 0.12 && wx < 0.88 && v > 0.10 && v < 0.62) {
            R = 40; G = 45; B = 60;
            double u = std::fmod((wx - 0.12) / 0.76 * 3.0, 1.0), vv = (v - 0.10) / 0.52;
            int r2, g2, b2;
            if (ornek(it, u, vv, r2, g2, b2)) { R = r2; G = g2; B = b2; }
        } else if (wx > 0.25 && wx < 0.65 && v > 0.70 && v < 0.84) { R = 15; G = 15; B = 20; }
        else if (wx > 0.76 && wx < 0.86 && v > 0.68 && v < 0.76) { R = 245; G = 205; B = 40; }
        else if (wx > 0.72 && wx < 0.90 && v > 0.80 && v < 0.92) { R = 190; G = 190; B = 200; }
    } else if (tip == T_OTO_KIRIK) {       // parcalanmis otomat
        R = 95; G = 95; B = 100;
        if (wx < 0.06 || wx > 0.94 || v < 0.04) { R = 50; G = 50; B = 55; }
        else if (wx > 0.12 && wx < 0.88 && v > 0.10 && v < 0.62) {
            R = 18; G = 18; B = 22;
            if (std::fabs(wx - 0.12 - (v - 0.10) * 1.46) < 0.03 || std::fabs(wx - 0.88 + (v - 0.10) * 1.46) < 0.03) { R = 200; G = 200; B = 205; }
        }
    } else if (tip == T_CETVEL) {          // cetvel sikismis sari kapi
        R = 238; G = 196; B = 40;
        if (wx < 0.07 || wx > 0.93 || v < 0.05) { R = 150; G = 110; B = 15; }
        else if (v > 0.12 && v < 0.30 && wx > 0.2 && wx < 0.8) { R = 255; G = 240; B = 160; }
        if (v > 0.44 && v < 0.60) {                                            // kapinin arasindaki cetvel
            R = 230; G = 195; B = 115;
            if (v < 0.455 || v > 0.585) { R = 120; G = 85; B = 40; }
            else if (std::fmod(wx * 20.0, 1.0) < 0.12) { R = 60; G = 40; B = 20; }   // cetvel cizgileri
        }
    } else if (tip == T_KILIT) {           // kilitli ceza kapisi
        R = 95; G = 95; B = 110;
        if (wx < 0.08 || wx > 0.92 || v < 0.06) { R = 50; G = 50; B = 60; }
        if (v > 0.45 && v < 0.55) { R = 200; G = 50; B = 50; }
        if (std::hypot(wx - 0.5, v - 0.5) < 0.09) { R = 245; G = 205; B = 40; }
    } else tabanDuvar(wx, v, R, G, B);
}

SDL_FRect slotRect(int i) { return {SW - 386.f + i * 64.f, 10.f, 56.f, 56.f}; }
// sira: 0 ileri, 1 geri, 2 sag, 3 sol
// elmas (arti) yerlesim: ust=ileri, alt=geri, sol=sol, sag=sag
const SDL_FRect btnYon[4] = {
    {92,  350, 76, 76},   // ileri (ust)
    {92,  514, 76, 76},   // geri  (alt)
    {174, 432, 76, 76},   // sag
    {10,  432, 76, 76}    // sol
};
const SDL_FRect btnKos = {SW - 112.f, SH - 310.f, 88, 88};
const SDL_FRect btnAksiyon = {SW - 112.f, SH - 210.f, 88, 88};
const SDL_FRect btnKullan = {SW - 112.f, SH - 110.f, 88, 88};

int main(int, char**) {
    klasorleriHazirla();
    std::srand((unsigned)std::time(nullptr));
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");   // fare ve dokunma ayri ayri islenir
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    if (!SDL_Init(SDL_INIT_VIDEO)) return 1;
    SDL_InitSubSystem(SDL_INIT_AUDIO);               // ses yoksa oyun sessiz calisir
    SDL_Window* pencere = SDL_CreateWindow("Okul Kacisi 3D", SW, SH, 0);
    SDL_Renderer* r = SDL_CreateRenderer(pencere, nullptr);
    if (!pencere || !r) return 1;
    SDL_SetRenderLogicalPresentation(r, SW, SH, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_Texture* tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888,
                                         SDL_TEXTUREACCESS_STREAMING, RW, RH);
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);

    {   // ses: kumanda bip'i icin kendi akisimiz
        SDL_AudioSpec spec;
        spec.format = SDL_AUDIO_S16; spec.channels = 1; spec.freq = 22050;
        sesAkis = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        if (sesAkis) SDL_ResumeAudioStreamDevice(sesAkis);
    }
#ifdef HAS_MIX
    if (!MIX_Init()) mixHata = std::string("MIX_Init: ") + SDL_GetError();
    else {
        mikser = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
        if (!mikser) mixHata = std::string("Mikser: ") + SDL_GetError();
        if (mikser) {
            std::string yd = dosyaBul("door.mp3"), yg = dosyaBul("git.mp3");
            if (!yd.empty()) kapiMp3 = sesAc(mikser, yd);
            if (!yg.empty()) gitMp3 = sesAc(mikser, yg);
            if (!yd.empty() && !kapiMp3) mixHata = std::string("door.mp3: ") + SDL_GetError();
            if (!yg.empty() && !gitMp3) mixHata = std::string("git.mp3: ") + SDL_GetError();
            if (gitMp3) { gitTrack = MIX_CreateTrack(mikser); if (gitTrack) MIX_SetTrackAudio(gitTrack, gitMp3); }
            std::string ys = dosyaBul("soda.mp3"), ye = dosyaBul("ebar.mp3"), yz = dosyaBul("skus.mp3"), yo = dosyaBul("otomat.mp3");
            std::string yt = dosyaBul("tehlike.mp3");
            if (!yt.empty()) tehlikeMp3 = sesAc(mikser, yt);
            if (!yt.empty() && !tehlikeMp3) mixHata = std::string("tehlike.mp3: ") + SDL_GetError();
            if (tehlikeMp3) { tehlikeTrack = MIX_CreateTrack(mikser); if (tehlikeTrack) MIX_SetTrackAudio(tehlikeTrack, tehlikeMp3); }
            std::string yu = dosyaBul("ucak.mp3");
            if (!yu.empty()) ucakMp3 = sesAc(mikser, yu);
            if (!yu.empty() && !ucakMp3) mixHata = std::string("ucak.mp3: ") + SDL_GetError();
            std::string yk2 = dosyaBul("kanca.mp3");
            if (!yk2.empty()) kancaMp3 = sesAc(mikser, yk2);
            if (!yk2.empty() && !kancaMp3) mixHata = std::string("kanca.mp3: ") + SDL_GetError();
            std::string yv = dosyaBul("vat.mp3");
            if (!yv.empty()) vatMp3 = sesAc(mikser, yv);
            if (!yv.empty() && !vatMp3) mixHata = std::string("vat.mp3: ") + SDL_GetError();
            if (vatMp3) { vatTrack = MIX_CreateTrack(mikser); if (vatTrack) MIX_SetTrackAudio(vatTrack, vatMp3); }
            std::string ydk = dosyaBul("dukkan.mp3");
            if (!ydk.empty()) dukkanMp3 = sesAc(mikser, ydk);
            if (!ydk.empty() && !dukkanMp3) mixHata = std::string("dukkan.mp3: ") + SDL_GetError();
            if (dukkanMp3) { dukkanTrack = MIX_CreateTrack(mikser); if (dukkanTrack) MIX_SetTrackAudio(dukkanTrack, dukkanMp3); }
            if (!yo.empty()) otomatMp3 = sesAc(mikser, yo);
            if (!yo.empty() && !otomatMp3) mixHata = std::string("otomat.mp3: ") + SDL_GetError();
            if (!yz.empty()) skusMp3 = sesAc(mikser, yz);
            if (!yz.empty() && !skusMp3) mixHata = std::string("skus.mp3: ") + SDL_GetError();
            if (!ys.empty()) sodaMp3 = sesAc(mikser, ys);
            if (!ye.empty()) ebarMp3 = sesAc(mikser, ye);
            if (!ys.empty() && !sodaMp3) mixHata = std::string("soda.mp3: ") + SDL_GetError();
            if (!ye.empty() && !ebarMp3) mixHata = std::string("ebar.mp3: ") + SDL_GetError();
            std::string ydg = dosyaBul("dogru.mp3");
            if (!ydg.empty()) dogruMp3 = sesAc(mikser, ydg);
            if (!ydg.empty() && !dogruMp3) mixHata = std::string("dogru.mp3: ") + SDL_GetError();
            std::string yf = dosyaBul("defter.mp3"), yk = dosyaBul("klasik.mp3");
            if (!yf.empty()) defterMp3 = sesAc(mikser, yf);
            if (!yk.empty()) klasikMp3 = sesAc(mikser, yk);
            if (!yf.empty() && !defterMp3) mixHata = std::string("defter.mp3: ") + SDL_GetError();
            if (!yk.empty() && !klasikMp3) mixHata = std::string("klasik.mp3: ") + SDL_GetError();
            if (klasikMp3) { klasikTrack = MIX_CreateTrack(mikser); if (klasikTrack) MIX_SetTrackAudio(klasikTrack, klasikMp3); }
            if (defterMp3) { defterTrack = MIX_CreateTrack(mikser); if (defterTrack) MIX_SetTrackAudio(defterTrack, defterMp3); }
            std::string yr = dosyaBul("rehber.mp3");
            if (!yr.empty()) rehberMp3 = sesAc(mikser, yr);
            if (!yr.empty() && !rehberMp3) mixHata = std::string("rehber.mp3: ") + SDL_GetError();
            if (rehberMp3) { rehberTrack = MIX_CreateTrack(mikser); if (rehberTrack) MIX_SetTrackAudio(rehberTrack, rehberMp3); }
        }
    }
#endif
    resimleriYukle(r);

    haritaKur();
    yeniden();
    {   // acilista durum raporu (ekranda ~20 sn)
        char* cd = SDL_GetCurrentDirectory();
        tani.push_back(std::string("Klasor: ") + (cd ? cd : "?"));
        if (cd) SDL_free(cd);
#ifdef HAS_IMG
        tani.push_back("SDL3_image: derlemede VAR");
#else
        tani.push_back("SDL3_image: derlemede YOK -> jpg acilmaz");
#endif
#ifdef HAS_MIX
        tani.push_back("SDL3_mixer: derlemede VAR");
#else
        tani.push_back("SDL3_mixer: derlemede YOK -> mp3 calmaz");
#endif
        std::string y1 = dosyaBul("1.jpg");
        tani.push_back(y1.empty() ? "1.jpg BULUNAMADI" : "1.jpg: " + y1);
        tani.push_back("Resim yuklenen: " + std::to_string(E_SAYI + 2 - resimEksik) + "/" + std::to_string(E_SAYI + 2));
        if (!ilkResimHata.empty()) tani.push_back("Resim hata: " + ilkResimHata);
#ifdef HAS_MIX
        tani.push_back(std::string("door: ") + (kapiMp3 ? "ok" : "YOK") + "  git: " + (gitMp3 ? "ok" : "YOK") +
                       "  defter: " + (defterMp3 ? "ok" : "YOK") + "  klasik: " + (klasikMp3 ? "ok" : "YOK") +
                       "  soda: " + (sodaMp3 ? "ok" : "YOK") + "  ebar: " + (ebarMp3 ? "ok" : "YOK") + "  skus: " + (skusMp3 ? "ok" : "YOK") + "  otomat: " + (otomatMp3 ? "ok" : "YOK") + "  vat: " + (vatMp3 ? "ok" : "YOK") + "  ucak: " + (ucakMp3 ? "ok" : "YOK") + "  kanca: " + (kancaMp3 ? "ok" : "YOK") + "  rehber: " + (rehberMp3 ? "ok" : "YOK") + "  tehlike: " + (tehlikeMp3 ? "ok" : "YOK") + "  dukkan: " + (dukkanMp3 ? "ok" : "YOK") + "  dogru: " + (dogruMp3 ? "ok" : "YOK"));
        if (!mixHata.empty()) tani.push_back("Ses hata: " + mixHata);
#endif
        taniSure = 0;                       // acilis raporu ekranda gosterilmez (sadece konsola yazilir)
        for (auto& l : tani) std::fprintf(stderr, "%s\n", l.c_str());
    }

    struct Fing { float x, y; bool ui; };
    std::map<SDL_FingerID, Fing> parmaklar;
    bool calisiyor = true;
    static double zbuf[RW], masaMesafe[RW];
    static float masaUst[RW];
    Uint64 onceki = SDL_GetTicks();

    auto mantiksal = [&](float nx, float ny, float& lx, float& ly) {
        int ww, wh; SDL_GetWindowSize(pencere, &ww, &wh);
        SDL_RenderCoordinatesFromWindow(r, nx * ww, ny * wh, &lx, &ly);
    };
    auto icinde = [](const SDL_FRect& b, float x, float y) {
        return x >= b.x && x <= b.x + b.w && y >= b.y && y <= b.y + b.h;
    };
    auto doldur = [&](float x, float y, float w, float h, int R, int G, int B, int A = 255) {
        SDL_SetRenderDrawColor(r, (Uint8)std::clamp(R, 0, 255), (Uint8)std::clamp(G, 0, 255),
                               (Uint8)std::clamp(B, 0, 255), (Uint8)A);
        SDL_FRect f = {x, y, w, h};
        SDL_RenderFillRect(r, &f);
    };
    auto bevel = [&](float x, float y, float w, float h, int R, int G, int B) {   // 3 boyutlu gorunum
        doldur(x, y, w, h, R * 45 / 100, G * 45 / 100, B * 45 / 100);
        doldur(x, y, w - 4, h - 4, R + 70, G + 70, B + 70);
        doldur(x + 4, y + 4, w - 8, h - 8, R, G, B);
    };
    auto yaz = [&](const std::string& s, float X, float Y, float k) {
        SDL_SetRenderScale(r, k, k);
        SDL_RenderDebugText(r, X / k, Y / k, s.c_str());
        SDL_SetRenderScale(r, 1, 1);
    };
    auto ikon = [&](int tip, float cx, float cy, float s) {   // esya gorselleri (resimden)
        if (img[tip].ok && img[tip].t) {
            float aw = (float)img[tip].w / img[tip].h, w, h;
            if (aw >= 1) { w = s; h = s / aw; } else { h = s; w = s * aw; }
            SDL_FRect d = {cx - w / 2, cy - h / 2, w, h};
            SDL_RenderTexture(r, img[tip].t, nullptr, &d);
        } else {
            doldur(cx - s * 0.3f, cy - s * 0.4f, s * 0.6f, s * 0.8f, esFB[tip][0], esFB[tip][1], esFB[tip][2]);
        }
    };

    // parmak / fare basildi: dugmelere, slotlara bak, yoksa "bakis surukleme" baslat
    const SDL_FingerID FARE_ID = (SDL_FingerID)0xFFFFFFF0u;
    auto basla = [&](SDL_FingerID id, float lx, float ly) {
        if (girisMenu) {                                    // giris menusu: Oyna / Rehber
            if (rehberAcik) {
                if (icinde(btnRMenu, lx, ly)) { rehberKapat(); parmaklar.clear(); return; }
                if (ly >= REH_LIST_Y && ly <= REH_LIST_Y + REH_LIST_H) {
                    if (lx >= REH_SB_X - 10) { rehberSurukle = 2; rehberThumbAyarla(ly); }     // kaydirma cubugu (genis dokunma alani)
                    else { rehberSurukle = 1; rehberSonY = ly; }                                // listeyi surukle
                    parmaklar[id] = {lx, ly, true};
                }
                return;
            }
            if (icinde(btnMOyna, lx, ly)) { menuOyna(); parmaklar.clear(); }
            else if (icinde(btnMRehber, lx, ly)) rehberAc();
            return;
        }
        if (durum != 0) { yeniden(); parmaklar.clear(); return; }
        if (kantinAcik) {                                   // kantin: urun satirlari ve geri tusu
            for (int i = 0; i < 5; i++) if (icinde(kantinSatir(i), lx, ly)) kantinAl(i);
            if (icinde(kantinGeri(), lx, ly)) kantinAcik = false;
            return;
        }
        if (nbMod == 2) {                                   // nobetci: renk kutulari
            for (int i = 0; i < 2; i++) if (icinde(nbDugme(i), lx, ly)) nobetTikla(i);
            return;
        }
        if (tahta.acik) {                                   // akilli tahta: sik secimi
            for (int i = 0; i < 3; i++) if (icinde(tahtaSecRect(i), lx, ly)) tahtaCevap(i);
            return;
        }
        bool ui = false;
        for (int i = 0; i < SLOT; i++)
            if (icinde(slotRect(i), lx, ly)) { sel = (sel == i) ? -1 : i; ui = true; }
        if (icinde(btnAksiyon, lx, ly)) { aksiyon(); ui = true; }
        if (sel >= 0 && slot[sel] >= 0 && icinde(btnKullan, lx, ly)) { kullan(); ui = true; }
        if (icinde(btnKos, lx, ly)) ui = true;
        for (int i = 0; i < 4; i++) if (icinde(btnYon[i], lx, ly)) ui = true;
        parmaklar[id] = {lx, ly, ui};
    };
    // parmak / fare kaydi: dugme disinda suruklenirse bakis yonu doner
    auto suruk = [&](SDL_FingerID id, float lx, float ly) {
        auto it = parmaklar.find(id);
        if (it == parmaklar.end()) return;
        if (rehberAcik) {                                   // rehber: listeyi / cubugu surukle
            if (rehberSurukle == 1) { rehberKay += rehberSonY - ly; rehberSonY = ly; rehberKayKisitla(); }
            else if (rehberSurukle == 2) rehberThumbAyarla(ly);
            it->second.y = ly;
            return;
        }
        if (!it->second.ui && durum == 0 && !tahta.acik && nbMod != 2 && !kantinAcik && !(kab.var && kab.o)) don((lx - it->second.x) * 0.006);
        it->second.x = lx; it->second.y = ly;
    };

    while (calisiyor) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) calisiyor = false;
            if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
                SDL_Scancode sc = e.key.scancode;
                if (girisMenu) {                                   // menu: Enter / Bosluk / E = Oyna, ESC = cikis
                    if (rehberAcik) {                              // rehber: ESC = menuye don, yon tuslari = kaydir
                        if (sc == SDL_SCANCODE_ESCAPE || sc == SDL_SCANCODE_BACKSPACE) rehberKapat();
                        else if (sc == SDL_SCANCODE_UP) rehberKay -= 56;
                        else if (sc == SDL_SCANCODE_DOWN) rehberKay += 56;
                        else if (sc == SDL_SCANCODE_PAGEUP) rehberKay -= 400;
                        else if (sc == SDL_SCANCODE_PAGEDOWN) rehberKay += 400;
                        else if (sc == SDL_SCANCODE_HOME) rehberKay = 0;
                        else if (sc == SDL_SCANCODE_END) rehberKay = rehberMaxKay();
                        rehberKayKisitla();
                    } else if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_SPACE || sc == SDL_SCANCODE_E) menuOyna();
                    else if (sc == SDL_SCANCODE_ESCAPE) calisiyor = false;
                    continue;
                }
                if (sc == SDL_SCANCODE_R) { yeniden(); parmaklar.clear(); }
                if (sc == SDL_SCANCODE_ESCAPE) calisiyor = false;
                if (kantinAcik) {                                   // kantin: 1-5 satin al, B / Backspace geri
                    if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_5) kantinAl(sc - SDL_SCANCODE_1);
                    if (sc == SDL_SCANCODE_B || sc == SDL_SCANCODE_BACKSPACE) kantinAcik = false;
                } else if (nbMod == 2) {                            // nobetci: A/Sol/Z = yesil, D/Sag/X = mavi
                    if (sc == SDL_SCANCODE_LEFT || sc == SDL_SCANCODE_A || sc == SDL_SCANCODE_Z) nobetTikla(0);
                    if (sc == SDL_SCANCODE_RIGHT || sc == SDL_SCANCODE_D || sc == SDL_SCANCODE_X) nobetTikla(1);
                } else if (tahta.acik) {
                    if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_3) tahtaCevap(sc - SDL_SCANCODE_1);
                } else if (sc == SDL_SCANCODE_E || sc == SDL_SCANCODE_SPACE) aksiyon();
                if (!tahta.acik && nbMod != 2 && !kantinAcik && sc == SDL_SCANCODE_F) kullan();
                if (!tahta.acik && nbMod != 2 && !kantinAcik && sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_6) {
                    int i = sc - SDL_SCANCODE_1;
                    sel = (sel == i) ? -1 : i;
                }
            }
            // dokunma (telefon)
            if (e.type == SDL_EVENT_FINGER_DOWN) {
                float lx, ly; mantiksal(e.tfinger.x, e.tfinger.y, lx, ly);
                basla((SDL_FingerID)e.tfinger.fingerID, lx, ly);
            }
            if (e.type == SDL_EVENT_FINGER_MOTION) {
                float lx, ly; mantiksal(e.tfinger.x, e.tfinger.y, lx, ly);
                suruk((SDL_FingerID)e.tfinger.fingerID, lx, ly);
            }
            if (e.type == SDL_EVENT_FINGER_UP || e.type == SDL_EVENT_FINGER_CANCELED)
                parmaklar.erase(e.tfinger.fingerID);
            // fare (bilgisayar): sol tus = parmak
            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.which != SDL_TOUCH_MOUSEID) {
                SDL_ConvertEventToRenderCoordinates(r, &e);
                basla(FARE_ID, e.button.x, e.button.y);
            }
            if (e.type == SDL_EVENT_MOUSE_MOTION && e.motion.which != SDL_TOUCH_MOUSEID) {
                SDL_ConvertEventToRenderCoordinates(r, &e);
                suruk(FARE_ID, e.motion.x, e.motion.y);
            }
            if (e.type == SDL_EVENT_MOUSE_WHEEL && girisMenu && rehberAcik) { rehberKay -= e.wheel.y * 60.0; rehberKayKisitla(); }
            if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.which != SDL_TOUCH_MOUSEID)
                parmaklar.erase(FARE_ID);
        }

        Uint64 simdi = SDL_GetTicks();
        double dt = (simdi - onceki) / 1000.0;
        if (dt > 0.1) dt = 0.1;
        onceki = simdi;

        if (girisMenu && rehberAcik) {                       // ===== REHBER =====
            if (parmaklar.empty()) rehberSurukle = 0;
            rehberKayKisitla();
            auto yazR = [&](const std::string& t, float X, float Y, float k, int R, int G, int B) {
                SDL_SetRenderDrawColor(r, (Uint8)R, (Uint8)G, (Uint8)B, 255);
                yaz(t, X, Y, k);
            };
            SDL_SetRenderDrawColor(r, 0, 0, 0, 255); SDL_RenderClear(r);
            doldur(0, 0, SW, SH, 28, 38, 78);
            SDL_Rect klip = {0, (int)REH_LIST_Y, SW, (int)REH_LIST_H};
            SDL_SetRenderClipRect(r, &klip);                                    // liste sadece kendi alaninda gorunur
            for (int t = 0; t < E_SAYI; t++) {
                float y0 = REH_LIST_Y + t * REH_ROW - (float)rehberKay;
                if (y0 + REH_ROW < REH_LIST_Y || y0 > REH_LIST_Y + REH_LIST_H) continue;
                doldur(16, y0 + 4, 746, REH_ROW - 8, 48, 60, 110);              // kart
                doldur(24, y0 + 10, 104, REH_ROW - 20, 28, 38, 78);             // sol: gorsel kutusu
                ikon(t, 76, y0 + REH_ROW / 2, 88);
                yazR(esAd[t], 142, y0 + 12, 2, 255, 220, 60);                   // sag: ad ve ne ise yaradigi
                for (size_t li = 0; li < REH_YAZI[t].size(); li++)
                    yazR(REH_YAZI[t][li], 142, y0 + 38 + li * 16.f, 1.5f, 230, 235, 250);
            }
            SDL_SetRenderClipRect(r, nullptr);
            doldur(REH_SB_X, REH_LIST_Y, 18, REH_LIST_H, 18, 24, 52);           // en sag: kaydirma cubugu
            bevel(REH_SB_X, rehberThumbY(), 18, rehberThumbH(), 150, 160, 205);
            doldur(0, 0, SW, REH_LIST_Y - 2, 28, 38, 78);                       // ust serit
            yazR("REHBER", 20, 16, 4, 255, 220, 60);
            yazR("Kaydir: surukle / tekerlek / yon tuslari", 230, 26, 1.5f, 170, 185, 220);
            bevel(btnRMenu.x, btnRMenu.y, btnRMenu.w, btnRMenu.h, 170, 60, 60);   // en ust sag: menuye git
            yazR("MENU", btnRMenu.x + (btnRMenu.w - 4 * 8.0f * 3) / 2, btnRMenu.y + (btnRMenu.h - 24) / 2, 3, 255, 255, 255);
            SDL_RenderPresent(r);
            SDL_Delay(8);
            continue;
        }
        if (girisMenu) {                                     // ===== GIRIS MENUSU =====
            const double mt = SDL_GetTicks() / 1000.0;
            auto yazM = [&](const std::string& t, float X, float Y, float k, int R, int G, int B) {
                SDL_SetRenderDrawColor(r, (Uint8)R, (Uint8)G, (Uint8)B, 255);
                yaz(t, X, Y, k);
            };
            SDL_SetRenderDrawColor(r, 0, 0, 0, 255); SDL_RenderClear(r);
            doldur(0, 0, SW, 400, 28, 38, 78);                                   // duvar
            doldur(0, 400, SW, 40, 60, 90, 150);                                 // mavi alt bant
            for (int cy2 = 0; cy2 < 4; cy2++)                                    // karolu zemin
                for (int cx2 = 0; cx2 < 20; cx2++) {
                    bool k2 = ((cx2 + cy2) & 1) != 0;
                    doldur(cx2 * 40.f, 440.f + cy2 * 40.f, 40, 40, k2 ? 150 : 125, k2 ? 130 : 108, k2 ? 100 : 85);
                }
            yazM("ZELTUS", (SW - 6 * 8.0f * 6) / 2, 36, 6, 255, 220, 60);
            yazM("Okul Kacisi", (SW - 11 * 8.0f * 2) / 2, 96, 2, 200, 210, 240);
            for (int i = 0; i < 2; i++) {                                        // sol: Lsoda ve Kalem, okuldaki gibi asagi yukari
                int tip = (i == 0) ? E_LSODA : E_KALEM;
                float cx = 120.f + i * 130.f;
                float bob = (float)std::sin(mt * 2.5 + i * 1.7) * 16.f;
                doldur(cx - 34, 396, 68, 8, 0, 0, 0, 90);                        // golge
                ikon(tip, cx, 290.f + bob, 190);
            }
            bevel(btnMOyna.x, btnMOyna.y, btnMOyna.w, btnMOyna.h, 50, 170, 80);   // OYNA
            yazM("OYNA", btnMOyna.x + (btnMOyna.w - 4 * 8.0f * 4) / 2, btnMOyna.y + (btnMOyna.h - 32) / 2, 4, 255, 255, 255);
            bevel(btnMRehber.x, btnMRehber.y, btnMRehber.w, btnMRehber.h, 60, 110, 200);   // REHBER
            yazM("REHBER", btnMRehber.x + (btnMRehber.w - 6 * 8.0f * 4) / 2, btnMRehber.y + (btnMRehber.h - 32) / 2, 4, 255, 255, 255);
            yazM("Dokun veya Enter", SW - 330.f, 430, 1.5f, 220, 230, 250);
            SDL_RenderPresent(r);
            SDL_Delay(8);
            continue;
        }

        bool tut[4] = {false, false, false, false};
        bool tutKos = false;
        for (auto& p : parmaklar) {
            for (int i = 0; i < 4; i++) if (icinde(btnYon[i], p.second.x, p.second.y)) tut[i] = true;
            if (icinde(btnKos, p.second.x, p.second.y)) tutKos = true;
        }

        if (taniSure > 0) taniSure -= dt;
        if (durum == 0) {
            const double dtGercek = dt;                 // kantin acikken dunya donar (hoca dahil herkes durur)
            if (kantinAcik) dt = 0;
            sure += dt;
            if (isimlikSure > 0) isimlikSure -= dt;
            if (yopSure > 0) yopSure -= dt;
            if (sonsuzSure > 0) sonsuzSure -= dt;
            if (hocaHizli > 0) hocaHizli -= dt;
            if (bildirimSure > 0) bildirimSure -= dt;
            if (cezaKalan > 0) {
                cezaKalan -= dt;
                if (cezaKalan <= 0) { cezaKalan = 0; harita[6][41] = T_MAVI_ACIK; bildir("Ceza bitti! Odadan cik."); }
            }
            bool odada = (px >= 37 && py < 6.0);          // ceza odasindayken muhafizlar bekler
            bool donmus = cezaKalan > 0 || odada;

            const bool* k = SDL_GetKeyboardState(nullptr);
            double ileri = 0, yan = 0, donus = 0;
            if (k[SDL_SCANCODE_W] || k[SDL_SCANCODE_UP] || tut[0]) ileri += 1;
            if (k[SDL_SCANCODE_S] || k[SDL_SCANCODE_DOWN] || tut[1]) ileri -= 1;
            if (k[SDL_SCANCODE_D]) yan += 1;
            if (k[SDL_SCANCODE_A]) yan -= 1;
            if (k[SDL_SCANCODE_RIGHT] || tut[2]) donus += 1;
            if (k[SDL_SCANCODE_LEFT] || tut[3]) donus -= 1;
            ileri = std::clamp(ileri, -1.0, 1.0);
            donus = std::clamp(donus, -1.0, 1.0);
            if (tahta.acik || nbMod == 2 || kantinAcik) { ileri = 0; yan = 0; donus = 0; }   // akilli tahtadayken yuruyemezsin

            // kos / enerji
            bool kosIstek = k[SDL_SCANCODE_LSHIFT] || k[SDL_SCANCODE_RSHIFT] || tutKos;
            bool hareket = (ileri != 0 || yan != 0);
            bool sonsuz = sonsuzSure > 0;
            kosuyor = kosIstek && hareket && !yorgun && (kosuEnerji > 0 || sonsuz);
            if (sonsuz) kosuEnerji = 100;
            else if (kosuyor) kosuEnerji -= 28 * dt;
            else kosuEnerji += 15 * dt;
            kosuEnerji = std::clamp(kosuEnerji, 0.0, 100.0);
            if (kosuEnerji <= 0) yorgun = true;
            if (yorgun && kosuEnerji >= 25) yorgun = false;

            bool tasiniyor = (kab.var && kab.o) || veletTasi || kanca.var;
            double hiz = (kosuyor ? 5.0 : 3.0) * dt;
            if (!tasiniyor) {
                if (donus != 0) don(donus * 2.2 * dt);
                double mx = (dx * ileri + (-dy) * yan) * hiz;
                double my = (dy * ileri + dx * yan) * hiz;
                if (serbest(px + mx, py)) px += mx;
                if (serbest(px, py + my)) py += my;
            }

            // Lsoda kabarcigi
            if (kab.var) {
                double adim = 4.5 * dt;
                kab.x += kab.ax * adim; kab.y += kab.ay * adim; kab.kalan -= adim;
                auto kabIcinde = [&](double x, double y) {
                    return std::fabs(x - kab.x) < 1.3 && std::fabs(y - kab.y) < 1.3 &&
                           x >= kab.bx0 && x < kab.bx1 + 1 && y >= kab.by0 && y < kab.by1 + 1;
                };
                if (!kab.h && kabIcinde(hx, hy)) kab.h = true;
                if (!kab.m && isimlikSure <= 0 && kabIcinde(ux, uy)) kab.m = true;
                if (!kab.o && !donmus && kabIcinde(px, py)) kab.o = true;
                if (kab.h) { hx = kab.x; hy = kab.y; }
                if (kab.m) { ux = kab.x; uy = kab.y; }
                if (kab.o) { px = kab.x; py = kab.y; }
                if (kab.kalan <= 0) {
                    if (kab.h) hocaDonuk = 4;
                    if (kab.m) uDonuk = 4;
                    kab.var = false;
                }
            }

            bfsHesapla(bfsP, (int)px, (int)py);

            // kovalamaca baslangici: klasik.mp3 calar, bitince (en fazla KLASIK_MAX sn) hoca kosmaya baslar
            if (kovaDurum == 1) {
                kovaSure += dt;
                if (klasikCaliyor()) klasikOynadi = true;
                bool bitti = klasikOynadi ? !klasikCaliyor() : kovaSure > 1.5;   // hic baslamadiysa 1.5sn bekle
                if (kovaSure >= KLASIK_MAX || bitti) {
                    kovaDurum = 2; klasikDurdur(); bildir("Hoca kovaliyor! Kac!");
                    hocayiUzaklastir(true);    // kovalamaca basladi: hoca haritanin EN UZAK noktasina isinlanir
                }
            }

            // yanlis cevap: 1-2 sn sonra akilli tahtadan at (defter alinmaz, hoca kovalar)
            if (tahta.acik && tahta.atSure > 0) {
                tahta.atSure -= dt;
                if (tahta.atSure <= 0) { tahta.acik = false; tahta = Tahta(); bildir("Tahtadan atildin! Kac!"); if (kovaDurum == 2) hocayiUzaklastir(); }
            }

            // ---- muz kabugu: kayan dusmanlar kabugun yonunde kayar, AI'lari o sirada durur ----
            const double oHx = hx, oHy = hy, oUx = ux, oUy = uy, oMx = mhx, oMy = mhy, oVx = vlx, oVy = vly;
            if (!donmus) {
                auto kay = [&](Kay& k, double& x, double& y) -> bool {         // true: kayma bu karede bitti
                    if (!k.var) return false;
                    double adim = std::min(MUZ_KAYMA_HIZ * dt, k.kalan);
                    double nx = x + k.ax * adim, ny = y + k.ay * adim;
                    if (adim > 1e-9 && serbest(nx, ny)) { x = nx; y = ny; k.kalan -= adim; } else k.kalan = 0;
                    if (k.kalan <= 1e-6) { k.var = false; return true; }
                    return false;
                };
                if (kab.var && kab.h) kayH.var = false;                        // kabarcik tasiyorsa kayma iptal
                if (kab.var && kab.m) kayU.var = false;
                if (kay(kayH, hx, hy)) { hocaDonuk = MUZ_SASIR; bildir("Hoca kaydi! Simdi kac!"); }
                if (kay(kayU, ux, uy)) { uDonuk = MUZ_SASIR; uAlarm = 0; }
                if (kay(kayM, mhx, mhy)) { mhwx = -1; }
                if (kay(kayV, vlx, vly)) { vDinlen = MUZ_SASIR; }
            }

            // hoca
            if (!donmus && !(kab.var && kab.h) && !kayH.var) {
                if (hocaDonuk > 0) hocaDonuk -= dt;
                else if (kovaDurum == 2) {
                    double hh = (HOCA_TEMEL_HIZ + HOCA_DEFTER_ARTIS * toplanan) * (hocaHizli > 0 ? 1.6 : 1.0);
                    hh = std::min(hh * (1.0 + HOCA_YANLIS_ARTIS * yanlisSayi), HOCA_MAX_HIZ);   // her yanlista biraz daha hizli
                    if (ihbarSure > 0 && std::hypot(hx - ihbarX, hy - ihbarY) > 0.8)   // muhbir haber verdi: o yere kos
                        adimAt(hx, hy, hKapi, bfsH, ihbarX, ihbarY, std::max(hh, std::min(hh * 1.7, HOCA_MAX_HIZ + 0.6)), dt, 1.0);
                    else
                        adimAt(hx, hy, hKapi, bfsP, px, py, hh, dt, 2.0);
                }
            }

            // mudur: kosana, personel odasina girene gelir; kumanda sesine kosar
            bool gorevli = isimlikSure <= 0;
            bool ihlal = false;
            if (!donmus && !(kab.var && kab.m) && !kayU.var) {
                if (uDonuk > 0) uDonuk -= dt;
                else {
                    int pcx = std::clamp((int)px, 0, MW - 1), pcy = std::clamp((int)py, 0, MH - 1);
                    int md = bfsP[std::clamp((int)uy, 0, MH - 1)][std::clamp((int)ux, 0, MW - 1)];
                    ihlal = md >= 0 && ((kosuyor && koridor[pcy][pcx] && md <= 22) || (personelH[pcy][pcx] && md <= 40));
                    if (kumandaSure > 0) {
                        kumandaSure -= dt;
                        bfsHesapla(bfsM, (int)kumandaX, (int)kumandaY);
                        adimAt(ux, uy, uKapi, bfsM, kumandaX, kumandaY, 3.6, dt, 1.2);
                    } else if (gorevli && ihlal) {
                        if (uAlarm <= 0) bildir("Mudur seni gordu! Kac!");
                        uAlarm = 5;
                        adimAt(ux, uy, uKapi, bfsP, px, py, 3.6, dt, 1.2);
                    } else if (gorevli && uAlarm > 0) {
                        uAlarm -= dt;
                        adimAt(ux, uy, uKapi, bfsP, px, py, 3.6, dt, 1.2);
                    } else {
                        uAlarm = 0;
                        if (uwx < 0 || std::hypot(ux - uwx, uy - uwy) < 0.8) {   // yeni devriye noktasi
                            for (int deneme = 0; deneme < 500; deneme++) {
                                int x = std::rand() % MW, y = std::rand() % MH;
                                if (koridor[y][x] && harita[y][x] == T_BOS && !(girisKilitli && x <= 6 && y >= 16 && y <= 17)) { uwx = x + 0.5; uwy = y + 0.5; break; }
                            }
                        }
                        if (uwx >= 0) {
                            bfsHesapla(bfsM, (int)uwx, (int)uwy);
                            adimAt(ux, uy, uKapi, bfsM, uwx, uwy, 2.4, dt, 1.2);
                        }
                    }
                    if (gorevli && cezaKalan <= 0 && std::hypot(ux - px, uy - py) < 0.7 && (uAlarm > 0 || ihlal))
                        mudurYakala();
                }
            }

            // muhbir: koridorlarda hizli dolasir, seni gorunce konumunu hocaya bildirir
            if (ihbarSure > 0) ihbarSure -= dt;
            if (ihbarTekrar > 0) ihbarTekrar -= dt;
            if (!donmus && !kayM.var) {
                if (mhwx < 0 || std::hypot(mhx - mhwx, mhy - mhwy) < 0.8) {
                    for (int deneme = 0; deneme < 500; deneme++) {
                        int x = std::rand() % MW, y = std::rand() % MH;
                        if (koridor[y][x] && harita[y][x] == T_BOS && !(girisKilitli && x <= 6 && y >= 16 && y <= 17)) { mhwx = x + 0.5; mhwy = y + 0.5; break; }
                    }
                }
                if (mhwx >= 0) {
                    bfsHesapla(bfsI, (int)mhwx, (int)mhwy);
                    adimAt(mhx, mhy, mhKapi, bfsI, mhwx, mhwy, 4.2, dt, 0.8);
                }
                if (kovaDurum == 2 && isimlikSure <= 0 && ihbarTekrar <= 0 && std::hypot(mhx - px, mhy - py) < 13.0 &&
                    gorusAcik(mhx, mhy, px, py)) {
                    if (ihbarSure <= 0) {
                        bildir("Muhbir seni gordu! Hoca geliyor!");
                        std::vector<Sint16> v;
                        tonEkle(v, 900, 0.08, 5000); tonEkle(v, 0, 0.03, 0); tonEkle(v, 600, 0.12, 5000);
                        sesGonder(v);
                    }
                    ihbarX = px; ihbarY = py; ihbarSure = 7.0; ihbarTekrar = 1.5;
                    bfsHesapla(bfsH, std::clamp((int)px, 0, MW - 1), std::clamp((int)py, 0, MH - 1));
                }
            }

            // kanca: seni hedef noktaya hizla ceker
            if (kanca.var) {
                if (donmus) kanca.var = false;
                else {
                    double mx = kanca.tx - px, my = kanca.ty - py, d = std::hypot(mx, my), adim = KANCA_HIZ * dt;
                    if (d <= adim + 0.05) { px = kanca.tx; py = kanca.ty; kanca.var = false; bildir("Kanca: vardin!"); }
                    else {
                        double nx = px + mx / d * adim, ny = py + my / d * adim;
                        if (serbest(nx, ny)) { px = nx; py = ny; } else kanca.var = false;
                    }
                }
            }

            // kagit ucak: ucar, duvara / kapiya carpinca veya menzil bitince iner; hoca dustugu yere kosar
            if (ucak.var) {
                double adim = UCAK_HIZ * dt;
                double nx = ucak.x + ucak.ax * adim, ny = ucak.y + ucak.ay * adim;
                if (ucak.kalan > 0 && serbest(nx, ny)) { ucak.x = nx; ucak.y = ny; ucak.kalan -= adim; }
                else {
                    ucak.var = false;
                    ihbarX = ucak.x; ihbarY = ucak.y; ihbarSure = UCAK_LURE; ihbarTekrar = UCAK_LURE;   // muhbir bu sure icinde ustune yazamaz
                    bfsHesapla(bfsH, std::clamp((int)ucak.x, 0, MW - 1), std::clamp((int)ucak.y, 0, MH - 1));
                    bildir("Ucak dustu! Hoca oraya kosuyor");
                }
            }

            // velet
            if (vDinlen > 0) vDinlen -= dt;
            if (veletOyalan > 0) veletOyalan -= dt;
            for (auto& dm : dumanlar) dm.sure -= dt;
            dumanlar.erase(std::remove_if(dumanlar.begin(), dumanlar.end(), [](const Duman& dm) { return dm.sure <= 0; }), dumanlar.end());
            if (donmus && veletTasi) { veletTasi = false; veletKos = false; vDinlen = VELET_DINLEN; }
            if (!donmus) {
                if (veletOyalan > 0 || kayV.var) {
                    veletKos = false;                          // sekere daldi / kayiyor: oldugu yerde durur
                } else if (veletTasi) {
                    double adim = VELET_TASI_HIZ * dt;
                    double nx = px + vTasiAx * adim, ny = py + vTasiAy * adim;
                    if (vTasiKalan > 0 && serbest(nx, ny)) { px = nx; py = ny; vTasiKalan -= adim; }
                    else { veletTasi = false; veletKos = false; vDinlen = VELET_DINLEN; bildir("Velet seni koridorun sonuna birakti!"); }
                    vlx = px - vTasiAx * 0.55; vly = py - vTasiAy * 0.55;
                } else {
                    int ortak = -1, ortakUz = -1;               // oyuncu ve velet ayni koridor parcasinda mi?
                    int pcx = (int)px, pcy = (int)py, vcx = (int)vlx, vcy = (int)vly;
                    for (size_t i = 0; i < segler.size(); i++) {
                        const Seg& sg = segler[i];
                        bool pi = pcx >= sg.x0 && pcx <= sg.x1 && pcy >= sg.y0 && pcy <= sg.y1;
                        bool vi = vcx >= sg.x0 && vcx <= sg.x1 && vcy >= sg.y0 && vcy <= sg.y1;
                        int uz = (sg.x1 - sg.x0) + (sg.y1 - sg.y0);
                        if (pi && vi && uz > ortakUz) { ortak = (int)i; ortakUz = uz; }
                    }
                    bool gor = ortak >= 0 && vDinlen <= 0 && cezaKalan <= 0 &&
                               std::hypot(vlx - px, vly - py) < VELET_GORUS && gorusAcik(vlx, vly, px, py);
                    if (gor) {
                        if (!veletKos) {
                            bildir("Velet seni gordu! Kac!");
                            std::vector<Sint16> v;
                            tonEkle(v, 500, 0.08, 5000); tonEkle(v, 0, 0.03, 0); tonEkle(v, 700, 0.10, 5000);
                            sesGonder(v);
                        }
                        veletKos = true; vSeg = ortak;
                        const Seg& sg = segler[ortak];
                        if (sg.yatay) { vTasiAx = (px >= vlx) ? 1 : -1; vTasiAy = 0; }
                        else { vTasiAx = 0; vTasiAy = (py >= vly) ? 1 : -1; }
                        bfsHesapla(bfsP, std::clamp((int)px, 0, MW - 1), std::clamp((int)py, 0, MH - 1));
                        adimAt(vlx, vly, vKapi, bfsP, px, py, VELET_KOS_HIZ, dt, 1.5);
                        bool kab_o = kab.var && kab.o;
                        if (std::hypot(vlx - px, vly - py) < 0.6 && !kab_o && sel >= 0 && slot[sel] == E_SEKER) {
                            // SECILI naneli seker: velet carpinca sekeri kapar, mutlu olur, 5sn oyalanir (seni goturmez)
                            tuket();
                            veletOyalan = SEKER_SURE; veletKos = false;
                            std::vector<Sint16> v; tonEkle(v, 1100, 0.04, 4500); tonEkle(v, 0, 0.02, 0); tonEkle(v, 1500, 0.06, 4500); tonEkle(v, 1900, 0.08, 4500);
                            sesGonder(v);
                            bildir("Velet naneli sekeri kapti ve mutlu oldu: 5sn!");
                        } else if (std::hypot(vlx - px, vly - py) < 0.6 && !kab_o) {       // carpti: koridorun sonuna goturur
                            veletTasi = true; veletKos = false;
                            double son = (vTasiAx != 0) ? (vTasiAx > 0 ? sg.x1 + 1 - 0.6 : sg.x0 + 0.6)
                                                        : (vTasiAy > 0 ? sg.y1 + 1 - 0.6 : sg.y0 + 0.6);
                            vTasiKalan = (vTasiAx != 0) ? std::fabs(son - px) : std::fabs(son - py);
                            bildir("Velet seni yakaladi!");
                            std::vector<Sint16> v; tonEkle(v, 250, 0.20, 6000); sesGonder(v);
                        }
                    } else {
                        veletKos = false;                      // gezinme: yavasca koridorlarda dolas
                        if (vwx < 0 || std::hypot(vlx - vwx, vly - vwy) < 0.8) {
                            for (int deneme = 0; deneme < 500; deneme++) {
                                int x = std::rand() % MW, y = std::rand() % MH;
                                if (koridor[y][x] && harita[y][x] == T_BOS && !(girisKilitli && x <= 6 && y >= 16 && y <= 17)) { vwx = x + 0.5; vwy = y + 0.5; break; }
                            }
                        }
                        if (vwx >= 0) {
                            bfsHesapla(bfsV, (int)vwx, (int)vwy);
                            adimAt(vlx, vly, vKapi, bfsV, vwx, vwy, VELET_GEZ_HIZ, dt, 1.5);
                        }
                    }
                }
            }

            // muz kabugu tetigi: yerdeki kabuga yaklasan hoca / mudur / muhbir / velet kayar (kabuk kaybolur)
            if (!donmus) {
                struct Hedef { Kay* k; double* x; double* y; double ox, oy; const char* ad; bool engel; };
                Hedef hd[4] = {{&kayH, &hx, &hy, oHx, oHy, "Hoca", kab.var && kab.h},
                               {&kayU, &ux, &uy, oUx, oUy, "Mudur", kab.var && kab.m},
                               {&kayM, &mhx, &mhy, oMx, oMy, "Muhbir", false},
                               {&kayV, &vlx, &vly, oVx, oVy, "Velet", false}};
                for (auto& e : esyalar) {
                    if (e.tip != E_MUZ || e.alindi) continue;
                    for (int hi = 0; hi < 4; hi++) {
                        Hedef& h = hd[hi];
                        if (h.k->var || h.engel) continue;
                        if (std::hypot(*h.x - e.x, *h.y - e.y) > MUZ_TETIK || !gorusAcik(e.x, e.y, *h.x, *h.y)) continue;
                        double mx = *h.x - h.ox, my = *h.y - h.oy;
                        if (std::hypot(mx, my) < 0.002) { mx = *h.x - px; my = *h.y - py; }    // duruyorsa senden uzaga kayar
                        bool yatay = std::fabs(mx) >= std::fabs(my);
                        h.k->ax = yatay ? (mx >= 0 ? 1 : -1) : 0; h.k->ay = yatay ? 0 : (my >= 0 ? 1 : -1);
                        double kal = 0;
                        for (double d = 0.1; d <= 60.0; d += 0.1) {                             // duvara / kapali kapiya kadar
                            if (!serbest(*h.x + h.k->ax * d, *h.y + h.k->ay * d)) break;
                            kal = d;
                        }
                        h.k->kalan = std::max(kal, 0.5); h.k->var = true;
                        e.alindi = true;
                        if (hi == 3 && veletTasi) { veletTasi = false; veletKos = false; }      // velet seni tasiyorsa birakir
                        bildir(std::string(h.ad) + " muz kabuguna basti!");
                        std::vector<Sint16> v; tonEkle(v, 1400, 0.05, 4500); tonEkle(v, 900, 0.06, 4500); tonEkle(v, 500, 0.10, 4500); sesGonder(v);
                        break;                                                                  // bir kabuk bir kez
                    }
                }
            }

            nobetGuncelle(dt, donmus);
            kopyaciGuncelle(dt, donmus);
            tehlikeGuncelle();
            balonGuncelle(dt);
            if (kantinMesajT > 0) kantinMesajT -= dtGercek;
            if (kantinAcik && (donmus || cezaKalan > 0 || nbMod == 2 || durum != 0)) kantinAcik = false;   // baska bir sey olursa kantin kapanir
            vatGuncelle();
            dukkanGuncelle();
            {
                bool kac = kovaDurum == 2 && !digerMuzikSus();      // kovalamaca muzigi: nobetci gelirken vat.mp3 calar
                gitGuncelle(kac);
            }
            for (auto& k : sariKapilar)                  // cetvel suresi dolunca kapi serbest kalir
                if (k.sikis > 0) {
                    k.sikis -= dt;
                    if (k.sikis <= 0) { k.sikis = 0; k.kilitli = false; k.acik = false; harita[k.y][k.x] = T_SARI; }
                }
            sariKapiGuncelle();
            if (kovaDurum == 2 && cezaKalan <= 0 && !odada && !kayH.var && std::hypot(hx - px, hy - py) < 0.6) durum = 2;
        }

        if (durum != 0) gitGuncelle(false);
        muzikGuncelle();

        // ================= 3B CIZIM (yazilim tamponu) =================
        // tavan (ampullu) ve zemin (karolu)
        {
            double rdx0 = dx - plx, rdy0 = dy - ply, rdx1 = dx + plx, rdy1 = dy + ply;
            for (int y = 0; y < RH; y++) {
                int p = y - RH / 2;
                if (p == 0) p = 1;
                double rowDist = (0.5 * RH) / std::abs(p);
                double sx = rowDist * (rdx1 - rdx0) / RW, sy = rowDist * (rdy1 - rdy0) / RW;
                double fx = px + rowDist * rdx0, fy = py + rowDist * rdy0;
                bool tavan = p < 0;
                double f = 1.0 / (1.0 + rowDist * 0.12);
                Uint32* satir = &pix[y * RW];
                for (int x = 0; x < RW; x++) {
                    int cx = (int)std::floor(fx), cy = (int)std::floor(fy);
                    double ux2 = fx - cx, uy2 = fy - cy;
                    if (tavan) {
                        int m = ((cx % 3) + 3) % 3, n = ((cy % 3) + 3) % 3;
                        if (m == 1 && n == 1 && ux2 > 0.25 && ux2 < 0.75 && uy2 > 0.25 && uy2 < 0.75)
                            satir[x] = rgb(255, 250, 215);                       // ampul
                        else {
                            double g = (ux2 < 0.04 || uy2 < 0.04) ? 0.7 : 1.0;    // tavan karosu
                            satir[x] = rgb((int)(200 * g * f), (int)(200 * g * f), (int)(208 * g * f));
                        }
                    } else {
                        bool kare = (((int)std::floor(fx * 2) + (int)std::floor(fy * 2)) & 1) != 0;
                        int a = kare ? 150 : 125, b = kare ? 130 : 108, c = kare ? 100 : 85;
                        satir[x] = rgb((int)(a * f), (int)(b * f), (int)(c * f));
                    }
                    fx += sx; fy += sy;
                }
            }
        }

        hedefTip = -1; hedefId = -1; hedefDist = 99;
        int kapiVar = 0; int kapiX = 0, kapiY = 0; double kapiD = 99;
        int makVar = 0; int makX = 0, makY = 0; double makD = 99;

        for (int x = 0; x < RW; x++) {
            double kx = 2.0 * x / RW - 1.0;
            double rdx = dx + plx * kx, rdy = dy + ply * kx;
            int mapX = (int)px, mapY = (int)py;
            double ddx = (rdx == 0) ? 1e30 : std::fabs(1.0 / rdx);
            double ddy = (rdy == 0) ? 1e30 : std::fabs(1.0 / rdy);
            int stepX, stepY; double sdx, sdy;
            if (rdx < 0) { stepX = -1; sdx = (px - mapX) * ddx; } else { stepX = 1; sdx = (mapX + 1.0 - px) * ddx; }
            if (rdy < 0) { stepY = -1; sdy = (py - mapY) * ddy; } else { stepY = 1; sdy = (mapY + 1.0 - py) * ddy; }

            struct Vurus { double d; int yan; };
            Vurus masalar[32]; int masaSay = 0;
            double duvarD = 50; int duvarYan = 0, duvarTip = T_DUVAR, dCX = 0, dCY = 0;
            bool ilkDolu = true;
            while (true) {
                int yan;
                if (sdx < sdy) { sdx += ddx; mapX += stepX; yan = 0; }
                else { sdy += ddy; mapY += stepY; yan = 1; }
                if (mapX < 0 || mapY < 0 || mapX >= MW || mapY >= MH) { duvarD = 50; duvarYan = yan; break; }
                int h = harita[mapY][mapX];
                if (h == T_BOS || h == T_SARI_ACIK) continue;
                double d = (yan == 0) ? (sdx - ddx) : (sdy - ddy);
                if (d < 0.05) d = 0.05;
                if (x == RW / 2 && ilkDolu) {
                    ilkDolu = false;
                    if ((h == T_MAVI || h == T_MAVI_ACIK) && !kapiVar) { kapiVar = 1; kapiX = mapX; kapiY = mapY; kapiD = d; }
                    if ((h == T_OTO_E || h == T_OTO_L) && !makVar) { makVar = 1; makX = mapX; makY = mapY; makD = d; }
                }
                if (h == T_MAVI_ACIK) continue;
                if (h == T_SIRA) { if (masaSay < 32) masalar[masaSay++] = {d, yan}; }
                else { duvarD = d; duvarYan = yan; duvarTip = h; dCX = mapX; dCY = mapY; break; }
            }
            zbuf[x] = duvarD;
            masaMesafe[x] = 1e30; masaUst[x] = (float)RH;

            double wallX = (duvarYan == 0) ? py + duvarD * rdy : px + duvarD * rdx;
            wallX -= std::floor(wallX);
            double lh = RH / duvarD;
            int top = (int)(RH / 2.0 - lh / 2.0), bot = (int)(RH / 2.0 + lh / 2.0);
            double f = 1.0 / (1.0 + duvarD * 0.15) * (duvarYan == 1 ? 0.75 : 1.0);
            for (int y = std::max(0, top); y <= std::min(RH - 1, bot); y++) {
                double v = (y - top) / std::max(1.0, lh);
                int R, G, B;
                duvarRenk(duvarTip, wallX, v, dCX, dCY, R, G, B);
                pix[y * RW + x] = rgb((int)(R * f), (int)(G * f), (int)(B * f));
            }

            // siralar / masalar (uzaktan yakina, alcak)
            for (int i = masaSay - 1; i >= 0; i--) {
                double md = masalar[i].d, mlh = RH / md;
                int mb = (int)(RH / 2.0 + mlh / 2.0), mt = mb - (int)(mlh * 0.38);
                double mf = 1.0 / (1.0 + md * 0.15) * (masalar[i].yan == 1 ? 0.75 : 1.0);
                for (int y = std::max(0, mt); y <= std::min(RH - 1, mb); y++) {
                    bool ust = (y - mt) < (mb - mt) * 0.12;
                    pix[y * RW + x] = ust ? rgb((int)(190 * mf), (int)(135 * mf), (int)(80 * mf))
                                          : rgb((int)(150 * mf), (int)(100 * mf), (int)(55 * mf));
                }
                if (i == 0) { masaMesafe[x] = md; masaUst[x] = (float)mt; }
            }
        }

        // ---------- sprite'lar ----------
        std::vector<Sp> sp;
        for (size_t i = 0; i < defterler.size(); i++)
            if (!defterler[i].alindi)
                sp.push_back({defterler[i].x, defterler[i].y, 0, (int)i, std::hypot(defterler[i].x - px, defterler[i].y - py)});
        for (size_t i = 0; i < esyalar.size(); i++)
            if (!esyalar[i].alindi)
                sp.push_back({esyalar[i].x, esyalar[i].y, 2, (int)i, std::hypot(esyalar[i].x - px, esyalar[i].y - py)});
        sp.push_back({hx, hy, 1, 0, std::hypot(hx - px, hy - py)});
        sp.push_back({ux, uy, 3, 0, std::hypot(ux - px, uy - py)});
        sp.push_back({mhx, mhy, 5, 0, std::hypot(mhx - px, mhy - py)});
        sp.push_back({vlx, vly, 6, 0, std::hypot(vlx - px, vly - py)});
        sp.push_back({nbx, nby, 8, 0, std::hypot(nbx - px, nby - py)});
        sp.push_back({kpx, kpy, 10, 0, std::hypot(kpx - px, kpy - py)});
        for (size_t i = 0; i < balonlar.size(); i++)
            sp.push_back({balonlar[i].x, balonlar[i].y, 11, (int)i, std::hypot(balonlar[i].x - px, balonlar[i].y - py)});
        for (size_t i = 0; i < makOdalar.size(); i++)
            sp.push_back({makOdalar[i].mx, makOdalar[i].my, 12, (int)i, std::hypot(makOdalar[i].mx - px, makOdalar[i].my - py)});
        if (kanca.var) sp.push_back({kanca.tx, kanca.ty, 14, 0, std::hypot(kanca.tx - px, kanca.ty - py)});
        sp.push_back({KANTINCI_X, KANTINCI_Y, 13, 0, std::hypot(KANTINCI_X - px, KANTINCI_Y - py)});
        if (ucak.var) sp.push_back({ucak.x, ucak.y, 9, 0, std::hypot(ucak.x - px, ucak.y - py)});
        for (size_t i = 0; i < dumanlar.size(); i++)
            sp.push_back({dumanlar[i].x, dumanlar[i].y, 7, (int)i, std::hypot(dumanlar[i].x - px, dumanlar[i].y - py)});
        if (kab.var) sp.push_back({kab.x, kab.y, 4, kab.diyet ? 1 : 0, std::hypot(kab.x - px, kab.y - py)});
        std::sort(sp.begin(), sp.end(), [](const Sp& a, const Sp& b) { return a.d > b.d; });
        double invDet = 1.0 / (plx * dy - dx * ply);
        double zaman = SDL_GetTicks() / 1000.0;

        nbEkGor = false;
        for (auto& s : sp) {
            double sx = s.x - px, sy = s.y - py;
            double tX = invDet * (dy * sx - dx * sy);
            double tY = invDet * (-ply * sx + plx * sy);
            if (tY <= 0.15) continue;
            int ex = (int)((RW / 2.0) * (1 + tX / tY));
            double lh = RH / tY;
            double wS, hS, lift;
            if (s.tip == 1) { wS = 0.5; hS = 0.9; lift = 0; }
            else if (s.tip == 3) { wS = 0.5; hS = 0.95; lift = 0; }
            else if (s.tip == 5) { wS = 0.30; hS = 1.0; lift = 0; }
            else if (s.tip == 6) { wS = 0.42; hS = 0.62; lift = 0; }
            else if (s.tip == 7) { wS = DUMAN_R * 1.9; hS = 1.1; lift = 0; }
            else if (s.tip == 8) { wS = 0.46; hS = 0.85; lift = 0; }
            else if (s.tip == 9) { wS = 0.30; hS = 0.16; lift = 0.5; }
            else if (s.tip == 10) { wS = 0.40; hS = 0.78; lift = 0; }
            else if (s.tip == 11) { wS = 0.34; hS = 0.46; lift = 0.30 + std::sin(zaman * 2.0 + balonlar[s.id].faz) * 0.05; }
            else if (s.tip == 12) { wS = 1.10; hS = 0.95; lift = 0; }
            else if (s.tip == 13) { wS = 1.00; hS = 0.95; lift = 0; }
            else if (s.tip == 14) { wS = 0.24; hS = 0.32; lift = 0.42; }
            else if (s.tip == 4) { wS = 2.1; hS = 0.95; lift = 0.02; }
            else if (s.tip == 0) { wS = 0.26; hS = 0.3; lift = 0.35 + std::sin(zaman * 2.5 + s.id) * 0.04; }
            else {
                int t = esyalar[s.id].tip;
                hS = esHS[t]; wS = hS * esAsp[t];
                lift = esyalar[s.id].lift + std::sin(zaman * 2.5 + s.id * 1.7) * 0.03;
            }
            double gen = lh * wS, yuk = lh * hS;
            float alt = (float)(RH / 2.0 + lh / 2.0 - lh * lift), ust = alt - (float)yuk;
            double f = 1.0 / (1.0 + tY * 0.15);
            int b0 = (int)(ex - gen / 2), b1 = (int)(ex + gen / 2);
            if (s.tip == 8) {                                    // kalem yazisi icin nobetcinin ekran konumu
                nbEkGor = ex >= 0 && ex < RW && tY < zbuf[ex];
                nbEkX = ex * (double)SW / RW; nbEkUst = ust * (double)SH / RH;
            }
            const int kabImg = (s.tip == 4 && s.id == 1 && img[IMG_DKAB].ok) ? IMG_DKAB : IMG_KAB;   // diyet lsoda: 1_3.jpg

            // nisangah (ekranin ortasi) bu nesnenin uzerinde mi?
            if ((s.tip == 0 || s.tip == 2 || s.tip == 11 || s.tip == 12 || s.tip == 13) && tY < 3.0 && tY < zbuf[RW / 2] &&
                RW / 2 >= b0 - 4 && RW / 2 <= b1 + 4 && RH / 2 >= ust - 8 && RH / 2 <= alt + 8) {
                hedefTip = (s.tip == 0) ? 0 : (s.tip == 11 ? 5 : (s.tip == 12 ? 6 : (s.tip == 13 ? 7 : 2))); hedefId = s.id; hedefDist = tY;
            }

            auto dikeyP = [&](int x, float y0, float y1, int R, int G, int B) {
                int a = std::max(0, (int)y0), b = std::min(RH - 1, (int)y1);
                Uint32 c = rgb((int)(R * f), (int)(G * f), (int)(B * f));
                for (int y = a; y <= b; y++) pix[y * RW + x] = c;
            };
            for (int x = b0; x <= b1; x++) {
                if (x < 0 || x >= RW || tY >= zbuf[x]) continue;
                double u = (x - b0) / std::max(1.0, gen);
                float a = alt;
                if (masaMesafe[x] < tY) a = std::min(a, masaUst[x]);       // alcak siralarin arkasinda kal
                if (s.tip == 0) {                                   // defter
                    if (u < 0.15) dikeyP(x, ust, a, 200, 110, 20);
                    else dikeyP(x, ust, a, 255, 225, 60);
                } else if (s.tip == 2) {                            // esya (resimden)
                    int t = esyalar[s.id].tip;
                    int ya = std::max(0, (int)ust), yb = std::min(RH - 1, (int)a);
                    for (int y = ya; y <= yb; y++) {
                        double v = (y - ust) / std::max(1.0, yuk);
                        int R, G, B;
                        if (ornek(t, u, v, R, G, B)) pix[y * RW + x] = rgb((int)(R * f), (int)(G * f), (int)(B * f));
                    }
                } else if (s.tip == 1) {                            // hoca
                    float bas = ust + (float)(yuk * 0.25);
                    if (u > 0.25 && u < 0.75) dikeyP(x, ust, std::min(a, bas), 235, 190, 150);
                    dikeyP(x, bas, a, 200, 40, 40);
                    if (u > 0.47 && u < 0.53) dikeyP(x, bas, std::min(a, ust + (float)(yuk * 0.6)), 30, 30, 90);
                    if ((u > 0.35 && u < 0.42) || (u > 0.58 && u < 0.65))
                        dikeyP(x, ust + (float)(yuk * 0.08), std::min(a, ust + (float)(yuk * 0.13)), 20, 20, 20);
                } else if (s.tip == 3) {                            // mudur: lacivert takim, gri sac, sari kravat
                    float bas = ust + (float)(yuk * 0.22);
                    if (u > 0.28 && u < 0.72) dikeyP(x, ust, std::min(a, bas), 230, 185, 145);
                    if (u > 0.25 && u < 0.75) dikeyP(x, ust, std::min(a, ust + (float)(yuk * 0.07)), 190, 190, 195);
                    dikeyP(x, bas, a, 35, 45, 95);
                    if (u > 0.46 && u < 0.54) dikeyP(x, bas, std::min(a, ust + (float)(yuk * 0.55)), 225, 195, 40);
                    if ((u > 0.36 && u < 0.43) || (u > 0.57 && u < 0.64))
                        dikeyP(x, ust + (float)(yuk * 0.10), std::min(a, ust + (float)(yuk * 0.14)), 20, 20, 20);
                } else if (s.tip == 5) {                            // muhbir: ince, sari ustlu, iki kafali
                    float bas = ust + (float)(yuk * 0.30);
                    // iki kafa: sol ve sag, ortada boyun
                    bool solK = u > 0.04 && u < 0.46, sagK = u > 0.54 && u < 0.96;
                    if (solK || sagK) dikeyP(x, ust, std::min(a, bas), 230, 185, 145);
                    if ((solK && u > 0.10 && u < 0.38) || (sagK && u > 0.62 && u < 0.90))
                        dikeyP(x, ust, std::min(a, ust + (float)(yuk * 0.05)), 60, 40, 20);        // sac
                    if ((u > 0.14 && u < 0.20) || (u > 0.28 && u < 0.34) || (u > 0.66 && u < 0.72) || (u > 0.80 && u < 0.86))
                        dikeyP(x, ust + (float)(yuk * 0.12), std::min(a, ust + (float)(yuk * 0.17)), 20, 20, 20);   // gozler
                    if (u > 0.40 && u < 0.60) dikeyP(x, ust + (float)(yuk * 0.18), std::min(a, bas), 215, 170, 130);  // boyun
                    if (u > 0.30 && u < 0.70) {
                        dikeyP(x, bas, std::min(a, ust + (float)(yuk * 0.66)), 245, 205, 40);        // sari ust
                        dikeyP(x, ust + (float)(yuk * 0.66), a, 55, 55, 70);                          // pantolon
                    }
                } else if (s.tip == 6) {                            // velet: kucuk, buyuk kafali, turuncu tisort
                    float bas = ust + (float)(yuk * 0.38);
                    if (u > 0.12 && u < 0.88) {
                        dikeyP(x, ust, std::min(a, bas), 240, 200, 160);                                 // buyuk kafa
                        dikeyP(x, ust, std::min(a, ust + (float)(yuk * 0.08)), 90, 55, 25);              // sac
                    }
                    if ((u > 0.28 && u < 0.40) || (u > 0.60 && u < 0.72))
                        dikeyP(x, ust + (float)(yuk * 0.14), std::min(a, ust + (float)(yuk * 0.22)), 20, 20, 20);   // gozler
                    if (u > 0.36 && u < 0.64)
                        dikeyP(x, ust + (float)(yuk * 0.28), std::min(a, ust + (float)(yuk * 0.32)), 190, 40, 40);  // genis gulus
                    if (veletOyalan > 0) {                                                                // mutlu: yanak, acik agiz, nane sekeri
                        if ((u > 0.14 && u < 0.26) || (u > 0.74 && u < 0.86))
                            dikeyP(x, ust + (float)(yuk * 0.20), std::min(a, ust + (float)(yuk * 0.26)), 255, 130, 150);
                        if (u > 0.40 && u < 0.60)
                            dikeyP(x, ust + (float)(yuk * 0.27), std::min(a, ust + (float)(yuk * 0.34)), 110, 25, 25);
                        if (u > 0.80 && u < 0.96) {
                            dikeyP(x, ust + (float)(yuk * 0.42), std::min(a, ust + (float)(yuk * 0.54)), 110, 235, 175);
                            dikeyP(x, ust + (float)(yuk * 0.47), std::min(a, ust + (float)(yuk * 0.49)), 255, 255, 255);
                        }
                    }
                    if (u > 0.22 && u < 0.78) {
                        dikeyP(x, bas, std::min(a, ust + (float)(yuk * 0.70)), 235, 125, 35);             // turuncu tisort
                        dikeyP(x, ust + (float)(yuk * 0.50), std::min(a, ust + (float)(yuk * 0.55)), 250, 250, 250);  // cizgi
                        dikeyP(x, ust + (float)(yuk * 0.70), std::min(a, ust + (float)(yuk * 0.88)), 50, 80, 170);    // sort
                    }
                    if ((u > 0.28 && u < 0.46) || (u > 0.54 && u < 0.72)) {
                        dikeyP(x, ust + (float)(yuk * 0.88), std::min(a, ust + (float)(yuk * 0.95)), 240, 200, 160);  // bacaklar
                        dikeyP(x, ust + (float)(yuk * 0.95), a, 40, 40, 40);                                           // ayakkabi
                    }
                } else if (s.tip == 14) {                           // kanca ucu: gri metal, kavisli
                    auto Y = [&](double fr) { return ust + (float)(yuk * fr); };
                    if (u > 0.44 && u < 0.56) dikeyP(x, Y(0.0), std::min(a, Y(0.72)), 150, 150, 165);       // govde
                    if (u > 0.25 && u < 0.75) dikeyP(x, Y(0.70), std::min(a, Y(0.82)), 150, 150, 165);      // alt kavis
                    if ((u > 0.25 && u < 0.38) || (u > 0.62 && u < 0.75)) dikeyP(x, Y(0.52), std::min(a, Y(0.82)), 150, 150, 165);   // sivri uclar
                    if (u > 0.40 && u < 0.60) dikeyP(x, Y(0.0), std::min(a, Y(0.08)), 200, 160, 90);        // ip bagi
                } else if (s.tip == 13) {                           // kantinci teyze: tezgahin arkasinda, onluklu, topuzlu
                    auto Y = [&](double fr) { return ust + (float)(yuk * fr); };
                    if (u > 0.30 && u < 0.70) {
                        dikeyP(x, Y(0.08), std::min(a, Y(0.30)), 238, 200, 170);                       // yuz
                        dikeyP(x, Y(0.03), std::min(a, Y(0.12)), 90, 55, 35);                          // sac
                    }
                    if (u > 0.40 && u < 0.60) dikeyP(x, Y(0.0), std::min(a, Y(0.07)), 90, 55, 35);     // topuz
                    if ((u > 0.38 && u < 0.46) || (u > 0.54 && u < 0.62)) dikeyP(x, Y(0.16), std::min(a, Y(0.20)), 30, 30, 30);   // gozler
                    if (u > 0.44 && u < 0.56) dikeyP(x, Y(0.24), std::min(a, Y(0.26)), 190, 50, 60);   // gulus
                    if (u > 0.24 && u < 0.76) dikeyP(x, Y(0.30), std::min(a, Y(0.64)), 70, 100, 165);  // elbise
                    if (u > 0.38 && u < 0.62) dikeyP(x, Y(0.36), std::min(a, Y(0.64)), 245, 245, 245); // onluk
                    if ((u > 0.10 && u < 0.20) || (u > 0.78 && u < 0.90)) dikeyP(x, Y(0.54), std::min(a, Y(0.62)), 220, 160, 70);   // tezgah ustundeki yiyecekler
                    if (u > 0.30 && u < 0.36) dikeyP(x, Y(0.56), std::min(a, Y(0.62)), 200, 60, 60);
                    if (u > 0.03 && u < 0.97) {
                        dikeyP(x, Y(0.62), std::min(a, Y(0.99)), 150, 95, 50);                          // tezgah
                        dikeyP(x, Y(0.62), std::min(a, Y(0.66)), 195, 135, 75);                         // tezgah ustu
                    }
                } else if (s.tip == 11) {                           // sayili balon
                    const Balon& bl = balonlar[s.id];
                    int ya = std::max(0, (int)ust), yb = std::min(RH - 1, (int)a);
                    int br = 150 + (bl.deger * 53) % 90, bg = 150 + (bl.deger * 97) % 90, bb = 150 + (bl.deger * 31) % 90;   // pastel renk
                    std::string tx = std::to_string(bl.deger);
                    int nG = (int)tx.size();
                    double cu = std::min(0.16, 0.72 / (nG * 4 - 1)), cv = cu * 0.72;
                    double bw = (nG * 4 - 1) * cu, bh = 5 * cv;
                    for (int y = ya; y <= yb; y++) {
                        double v = (y - ust) / std::max(1.0, yuk);
                        double ex_ = (u - 0.5) / 0.5, ey_ = (v - 0.42) / 0.42;
                        bool govde = ex_ * ex_ + ey_ * ey_ < 1.0;
                        bool ip = v > 0.84 && std::fabs(u - 0.5) < 0.03;
                        if (!govde && !ip) continue;
                        int R = br, G = bg, B = bb;
                        if (ip) { R = G = B = 235; }
                        else {
                            if (ex_ < -0.25 && ey_ < -0.25 && ex_ * ex_ + ey_ * ey_ < 0.55) { R = std::min(255, R + 50); G = std::min(255, G + 50); B = std::min(255, B + 50); }   // parlama
                            double lu = (u - 0.5 + bw / 2) / cu, lv = (v - 0.42 + bh / 2) / cv;
                            if (lu >= 0 && lu < nG * 4 - 1 && lv >= 0 && lv < 5) {
                                int gi = (int)lu / 4, gx = (int)lu % 4, gy = (int)lv;
                                if (gx < 3 && (FONT35[tx[gi] - '0'][gy] & (4 >> gx))) { R = 20; G = 20; B = 40; }   // rakam
                            }
                        }
                        pix[y * RW + x] = rgb((int)(R * f), (int)(G * f), (int)(B * f));
                    }
                } else if (s.tip == 12) {                           // makine: gri govde, ekran, yanip sonen lambalar, donen carklar
                    int ya = std::max(0, (int)ust), yb = std::min(RH - 1, (int)a);
                    for (int y = ya; y <= yb; y++) {
                        double v = (y - ust) / std::max(1.0, yuk);
                        if (!(u > 0.04 && u < 0.96 && v > 0.04 && v < 0.97)) continue;
                        int R = 120, G = 125, B = 135;
                        if (u < 0.08 || u > 0.92 || v < 0.08) { R = 80; G = 85; B = 95; }                    // cerceve
                        if (u > 0.14 && u < 0.86 && v > 0.12 && v < 0.40) {                                    // yesil ekran
                            R = 15; G = 70; B = 40;
                            if (((int)(v * 40) & 1) == 0) G += 18;
                        }
                        for (int k = 0; k < 3; k++) {                                                          // lambalar
                            double du = (u - (0.22 + 0.28 * k)) * 1.16, dv = v - 0.48;
                            if (du * du + dv * dv < 0.0020) {
                                bool yan = std::sin(zaman * 4.0 + k * 2.1) > 0;
                                R = yan ? 255 : 90; G = yan ? (k == 1 ? 220 : 80) : 50; B = 40;
                            }
                        }
                        for (int g = 0; g < 2; g++) {                                                          // donen carklar
                            double gx0 = g == 0 ? 0.30 : 0.68, gr = g == 0 ? 0.16 : 0.19, yon = g == 0 ? 1.0 : -1.0;
                            double du = (u - gx0) * 1.16, dv = v - 0.74;
                            double d = std::sqrt(du * du + dv * dv);
                            double ang = std::atan2(dv, du);
                            double dis = gr * (1.0 + 0.16 * std::cos(8.0 * ang + yon * zaman * 1.5));          // disli kenar
                            if (d < dis) {
                                R = g == 0 ? 205 : 225; G = g == 0 ? 160 : 190; B = 55;
                                if (d < gr * 0.35) { R = 50; G = 50; B = 60; }                                 // eksen deligi
                            }
                        }
                        pix[y * RW + x] = rgb((int)(R * f), (int)(G * f), (int)(B * f));
                    }
                } else if (s.tip == 10) {                           // kopyaci cocuk: kirmizi sapka, goz maskesi, yesil kapsonlu
                    float bas = ust + (float)(yuk * 0.26);
                    if (u > 0.28 && u < 0.72) {
                        dikeyP(x, ust, std::min(a, bas), 235, 200, 165);                                   // yuz
                        dikeyP(x, ust, std::min(a, ust + (float)(yuk * 0.08)), 200, 40, 40);               // kirmizi sapka
                        dikeyP(x, ust + (float)(yuk * 0.10), std::min(a, ust + (float)(yuk * 0.17)), 15, 15, 15);   // goz maskesi
                        if ((u > 0.36 && u < 0.43) || (u > 0.57 && u < 0.64))
                            dikeyP(x, ust + (float)(yuk * 0.12), std::min(a, ust + (float)(yuk * 0.15)), 255, 255, 255);   // gozler
                        if (u > 0.40 && u < 0.60)
                            dikeyP(x, ust + (float)(yuk * 0.19), std::min(a, ust + (float)(yuk * 0.21)), 170, 40, 40);     // sinsi gulus
                    }
                    if (u > 0.20 && u < 0.80) dikeyP(x, bas, std::min(a, ust + (float)(yuk * 0.68)), 55, 135, 80);          // yesil kapson
                    if (kpMod == 2 && kpTip >= 0 && u > 0.60 && u < 0.84)                                                   // calinan esya (renkli paket)
                        dikeyP(x, ust + (float)(yuk * 0.42), std::min(a, ust + (float)(yuk * 0.62)), esFB[kpTip][0], esFB[kpTip][1], esFB[kpTip][2]);
                    if (u > 0.26 && u < 0.74) dikeyP(x, ust + (float)(yuk * 0.68), std::min(a, ust + (float)(yuk * 0.93)), 50, 70, 120);   // kot pantolon
                    if ((u > 0.28 && u < 0.46) || (u > 0.54 && u < 0.72)) dikeyP(x, ust + (float)(yuk * 0.93), a, 235, 235, 235);          // spor ayakkabi
                } else if (s.tip == 9) {                            // kagit ucak: beyaz, sivri burunlu ucgen
                    double yar = 0.5 * (1.0 - u * 0.9);                      // burna dogru daralir
                    float orta = ust + (float)(yuk * 0.5);
                    dikeyP(x, orta - (float)(yuk * yar), std::min(a, orta + (float)(yuk * yar)), 250, 250, 250);
                    dikeyP(x, orta + (float)(yuk * yar * 0.35), std::min(a, orta + (float)(yuk * yar * 0.5)), 185, 190, 205);   // katlama golgesi
                } else if (s.tip == 8) {                            // nobetci ogrenci: gozluk, okul formasi, elinde kagit
                    float bas = ust + (float)(yuk * 0.26);
                    if (u > 0.28 && u < 0.72) {
                        dikeyP(x, ust, std::min(a, bas), 240, 205, 170);                                  // yuz
                        dikeyP(x, ust, std::min(a, ust + (float)(yuk * 0.07)), 25, 20, 20);               // sac
                    }
                    if (!nbUzgun) {
                    if ((u > 0.30 && u < 0.46) || (u > 0.54 && u < 0.70))
                        dikeyP(x, ust + (float)(yuk * 0.10), std::min(a, ust + (float)(yuk * 0.17)), 20, 20, 30);   // gozluk cercevesi
                    if (u > 0.46 && u < 0.54)
                        dikeyP(x, ust + (float)(yuk * 0.115), std::min(a, ust + (float)(yuk * 0.13)), 20, 20, 30);  // gozluk koprusu
                    if ((u > 0.33 && u < 0.43) || (u > 0.57 && u < 0.67))
                        dikeyP(x, ust + (float)(yuk * 0.12), std::min(a, ust + (float)(yuk * 0.15)), 150, 200, 235); // cam
                    } else {                                                                                         // uzgun: gozluk burnun ucuna inmis
                        if ((u > 0.35 && u < 0.41) || (u > 0.59 && u < 0.65))
                            dikeyP(x, ust + (float)(yuk * 0.09), std::min(a, ust + (float)(yuk * 0.13)), 25, 20, 30);   // gozler (gozlugun ustunden bakar)
                        if (u > 0.32 && u < 0.45) {                                                                  // uzgun kas: ic uclar yukarida
                            float kb = (float)(0.075 - 0.02 * (u - 0.32) / 0.13);
                            dikeyP(x, ust + (float)(yuk * kb), std::min(a, ust + (float)(yuk * (kb + 0.012))), 60, 40, 30);
                        }
                        if (u > 0.55 && u < 0.68) {
                            float kb = (float)(0.075 - 0.02 * (0.68 - u) / 0.13);
                            dikeyP(x, ust + (float)(yuk * kb), std::min(a, ust + (float)(yuk * (kb + 0.012))), 60, 40, 30);
                        }
                        if ((u > 0.30 && u < 0.46) || (u > 0.54 && u < 0.70))
                            dikeyP(x, ust + (float)(yuk * 0.15), std::min(a, ust + (float)(yuk * 0.21)), 20, 20, 30);   // inik gozluk cercevesi
                        if (u > 0.46 && u < 0.54)
                            dikeyP(x, ust + (float)(yuk * 0.165), std::min(a, ust + (float)(yuk * 0.178)), 20, 20, 30); // koprusu
                        if ((u > 0.33 && u < 0.43) || (u > 0.57 && u < 0.67))
                            dikeyP(x, ust + (float)(yuk * 0.168), std::min(a, ust + (float)(yuk * 0.198)), 150, 200, 235); // cam
                        if (u > 0.42 && u < 0.58) {                                                                  // somurtan agiz
                            float ay = (u > 0.46 && u < 0.54) ? 0.228f : 0.240f;
                            dikeyP(x, ust + (float)(yuk * ay), std::min(a, ust + (float)(yuk * (ay + 0.012))), 150, 55, 55);
                        }
                        if (u > 0.31 && u < 0.34)
                            dikeyP(x, ust + (float)(yuk * 0.21), std::min(a, ust + (float)(yuk * 0.255)), 110, 180, 245); // gozyasi
                    }
                    if (u > 0.22 && u < 0.78) dikeyP(x, bas, std::min(a, ust + (float)(yuk * 0.70)), 30, 45, 110);      // lacivert ceket
                    if (u > 0.42 && u < 0.58) dikeyP(x, bas, std::min(a, ust + (float)(yuk * 0.40)), 245, 245, 245);    // beyaz gomlek
                    if (u > 0.47 && u < 0.53) dikeyP(x, bas, std::min(a, ust + (float)(yuk * 0.42)), 170, 30, 40);      // kravat
                    if (u > 0.30 && u < 0.70) {
                        dikeyP(x, ust + (float)(yuk * 0.44), std::min(a, ust + (float)(yuk * 0.68)), 250, 250, 245);    // elindeki kagit
                        if (u < 0.38) dikeyP(x, ust + (float)(yuk * 0.62), std::min(a, ust + (float)(yuk * 0.68)), 40, 170, 70);   // sol alt yesil
                        if (u > 0.62) dikeyP(x, ust + (float)(yuk * 0.62), std::min(a, ust + (float)(yuk * 0.68)), 50, 90, 230);   // sag alt mavi
                    }
                    if (u > 0.26 && u < 0.74) dikeyP(x, ust + (float)(yuk * 0.70), std::min(a, ust + (float)(yuk * 0.93)), 25, 25, 45);   // pantolon
                    if ((u > 0.30 && u < 0.46) || (u > 0.54 && u < 0.70)) dikeyP(x, ust + (float)(yuk * 0.93), a, 30, 30, 30);            // ayakkabi
                } else if (s.tip == 7) {                            // beyaz duman: yari saydam, dalgalanan bulut
                    double fade = std::min(1.0, dumanlar[s.id].sure / 1.5);
                    double tt = 2.0 * u - 1.0, kk = std::sqrt(std::max(0.0, 1.0 - tt * tt));
                    int ya = std::max(0, (int)ust), yb = std::min(RH - 1, (int)a);
                    for (int y = ya; y <= yb; y++) {
                        double v = (y - ust) / std::max(1.0, yuk);
                        double en = std::fabs(v - 0.5) * 2.0;
                        double kenar = kk * (1.0 - en * en);                    // ortada yogun, kenarda seyrek
                        if (kenar <= 0.02) continue;
                        double dalga = 0.5 + 0.5 * std::sin(zaman * 1.6 + u * 9.0 + v * 7.0 + s.id * 2.0);
                        int al = (int)(215 * kenar * (0.55 + 0.45 * dalga) * fade);
                        karistir(pix[y * RW + x], 245, 245, 250, std::clamp(al, 0, 255));
                    }
                } else if (img[kabImg].ok) {                        // lsoda kabarcigi: 8.jpg / diyet: 1_3.jpg (beyazlar seffaf)
                    int ya = std::max(0, (int)ust), yb = std::min(RH - 1, (int)a);
                    for (int y = ya; y <= yb; y++) {
                        double v = (y - ust) / std::max(1.0, yuk);
                        int R, G, B;
                        if (ornek(kabImg, u, v, R, G, B)) karistir(pix[y * RW + x], (int)(R * f), (int)(G * f), (int)(B * f), 255);
                    }
                } else {                                            // lsoda kabarcigi (resim yoksa yari saydam)
                    double tt = 2.0 * u - 1.0, kk = std::sqrt(std::max(0.0, 1.0 - tt * tt));
                    float cyy = (ust + alt) / 2, hh = (float)((alt - ust) / 2 * kk);
                    int ya = std::max(0, (int)(cyy - hh)), yb = std::min(RH - 1, (int)(cyy + hh));
                    for (int y = ya; y <= yb; y++) {
                        double en = std::fabs(y - cyy) / std::max(1.0f, hh);
                        if (en > 0.88 || kk < 0.35) karistir(pix[y * RW + x], 225, 242, 255, 160);
                        else karistir(pix[y * RW + x], 150, 210, 255, 60);
                    }
                }
            }
        }
        // kapi / otomat hedefi
        if (kapiVar && kapiD < 2.0 && (hedefTip == -1 || kapiD < hedefDist)) {
            hedefTip = 3; hedefKX = kapiX; hedefKY = kapiY; hedefDist = kapiD;
        }
        if (makVar && makD < 2.2 && (hedefTip == -1 || makD < hedefDist)) {
            hedefTip = 4; hedefKX = makX; hedefKY = makY; hedefDist = makD;
        }

        SDL_UpdateTexture(tex, nullptr, pix.data(), RW * 4);
        SDL_RenderTexture(r, tex, nullptr, nullptr);

        // ================= ARAYUZ =================
        float qo = 0;
        if (cozulenDefter < 2 && !quizAtla) {
            yaz("2 Tane Defter Coz: " + std::to_string(cozulenDefter) + "/2", 10, 8, 2);
            qo = 24;
        }
        yaz("Defter: " + std::to_string(toplanan) + "/" + std::to_string(ODA_SAYISI), 10, 10 + qo, 2);
        yaz("Kurus: " + std::to_string(kurusSayi()), 10, 32 + qo, 1.5f);
        // kos enerjisi
        doldur(10, 52 + qo, 144, 12, 0, 0, 0, 170);
        doldur(12, 54 + qo, 140.0f * (float)(kosuEnerji / 100.0), 8, yorgun ? 210 : (sonsuzSure > 0 ? 80 : 70),
               yorgun ? 70 : (sonsuzSure > 0 ? 200 : 210), yorgun ? 60 : (sonsuzSure > 0 ? 255 : 90));
        yaz(sonsuzSure > 0 ? "SINIRSIZ" : (yorgun ? "YORGUN" : "KOSU"), 160, 54 + qo, 1);
        float ly2 = 72 + qo;
        yaz("Makine Soru: " + std::to_string(msPuan) + " MS", 10, ly2, 1.5f); ly2 += 16;
        if (elBalon >= 0) { yaz("Elinde balon: " + std::to_string(elBalon), 10, ly2, 1.5f); ly2 += 16; }
        for (const auto& mo : makOdalar)                                       // makine odasinda: o odanin sorusu ekranda
            if (px >= mo.x0 && px < mo.x0 + 5 && py >= mo.y0 && py < mo.y0 + 5) {
                doldur(SW / 2 - 170, 70, 340, 30, 0, 0, 0, 170);
                yaz("MAKINE: " + mo.metin, SW / 2 - 160, 78, 2);
                break;
            }
        if (durum == 0 && kovaDurum == 1) { yaz("Hoca uyaniyor...", 10, ly2, 1.5f); ly2 += 16; }
        if (isimlikSure > 0) { yaz("Isimlik: " + std::to_string((int)isimlikSure + 1) + "s", 10, ly2, 1.5f); ly2 += 16; }
        if (yopSure > 0) { yaz("Yop (sessiz): " + std::to_string((int)yopSure + 1) + "s", 10, ly2, 1.5f); ly2 += 16; }
        if (hocaDonuk > 0) { yaz("Hoca sasirdi: " + std::to_string((int)hocaDonuk + 1) + "s", 10, ly2, 1.5f); ly2 += 16; }
        if (nbEtiketT > 0 && nbEkGor) {                                    // kalem: "Nobetci kagidimi mahvettin !"
            const std::string m = "Nobetci kagidimi mahvettin !";
            float w = (float)m.size() * 12.0f;
            float bx = std::clamp((float)nbEkX - w / 2, 6.0f, SW - w - 6.0f);
            float by = std::max((float)nbEkUst - 36.0f, 6.0f);
            doldur(bx - 6, by - 4, w + 12, 24, 0, 0, 0, 180);
            yaz(m, bx, by, 1.5f);
        }
        if (veletOyalan > 0) { yaz("Velet nane yiyor: " + std::to_string((int)veletOyalan + 1) + "s", 10, ly2, 1.5f); ly2 += 16; }
        if (uAlarm > 0) { yaz("MUDUR PESINDE!", 10, ly2, 1.5f); ly2 += 16; }
        if (kumandaSure > 0) { yaz("Mudur kumandaya kosuyor", 10, ly2, 1.5f); ly2 += 16; }

        if (cezaKalan > 0) {
            doldur(0, 0, SW, SH, 0, 0, 0, 60);
            std::string c1 = "CEZA: " + std::to_string((int)cezaKalan + 1) + " sn";
            yaz(c1, SW / 2.0f - (float)c1.size() * 12.0f, 80, 3);
            std::string c2 = "Mudur seni yakaladi! (" + std::to_string(cezaSayi) + ". kez)";
            yaz(c2, SW / 2.0f - (float)c2.size() * 6.0f, 112, 1.5f);
        }
        if (bildirimSure > 0 && !bildirim.empty()) {
            float w = (float)bildirim.size() * 12.0f;
            doldur(SW / 2.0f - w / 2 - 8, 140, w + 16, 24, 0, 0, 0, 160);
            yaz(bildirim, SW / 2.0f - w / 2, 146, 1.5f);
        }

        // envanter (6 slot, 3B gorunum)
        for (int i = 0; i < SLOT; i++) {
            SDL_FRect s = slotRect(i);
            if (i == sel) bevel(s.x, s.y, s.w, s.h, 200, 40, 40);     // secili: arka plan kirmizi
            else bevel(s.x, s.y, s.w, s.h, 95, 95, 110);
            if (slot[i] >= 0) {
                ikon(slot[i], s.x + s.w / 2, s.y + s.h / 2, 44);
                if (adet[i] > 1) yaz("x" + std::to_string(adet[i]), s.x + s.w - 24, s.y + s.h - 14, 1.5f);
            }
            yaz(std::to_string(i + 1), s.x + 5, s.y + 5, 1);
        }
        if (sel >= 0 && slot[sel] >= 0) {      // secili esyanin adi ve ne ise yaradigi
            yaz(std::string(esAd[slot[sel]]) + ": " + esAcik[slot[sel]], SW - 386.f, 72, 1);
        }

        // yon dugmeleri (sol alt, elmas dizilim)
        const char* semb[4] = {"^", "v", ">", "<"};
        for (int i = 0; i < 4; i++) {
            const SDL_FRect& b = btnYon[i];
            if (tut[i]) bevel(b.x, b.y, b.w, b.h, 140, 140, 160);
            else bevel(b.x, b.y, b.w, b.h, 80, 80, 95);
            yaz(semb[i], b.x + b.w / 2 - 16, b.y + b.h / 2 - 16, 4);
        }

        // kos dugmesi
        if (tutKos) bevel(btnKos.x, btnKos.y, btnKos.w, btnKos.h, 230, 160, 50);
        else bevel(btnKos.x, btnKos.y, btnKos.w, btnKos.h, yorgun ? 120 : 190, yorgun ? 70 : 120, 40);
        yaz("KOS", btnKos.x + 14, btnKos.y + 36, 3);

        // eylem dugmesi
        if (hedefTip != -1) bevel(btnAksiyon.x, btnAksiyon.y, btnAksiyon.w, btnAksiyon.h, 50, 170, 80);
        else bevel(btnAksiyon.x, btnAksiyon.y, btnAksiyon.w, btnAksiyon.h, 80, 80, 95);
        yaz("EYLEM", btnAksiyon.x + 4, btnAksiyon.y + 36, 2);

        // kullan dugmesi (secili slottaki esya varsa)
        if (sel >= 0 && slot[sel] >= 0) {
            bevel(btnKullan.x, btnKullan.y, btnKullan.w, btnKullan.h, 200, 40, 40);
            ikon(slot[sel], btnKullan.x + btnKullan.w / 2, btnKullan.y + 36, 56);
            yaz("KULLAN", btnKullan.x + 12, btnKullan.y + 68, 1.5f);
        }

        // ekranin ortasinda nokta (nisangah)
        doldur(SW / 2 - 4, SH / 2 - 4, 8, 8, 0, 0, 0, 200);
        if (hedefTip != -1) doldur(SW / 2 - 3, SH / 2 - 3, 6, 6, 80, 255, 120);
        else doldur(SW / 2 - 3, SH / 2 - 3, 6, 6, 255, 255, 255);
        if (hedefTip != -1) {
            std::string ad;
            if (hedefTip == 0) ad = "Defter";
            else if (hedefTip == 3) ad = "Kapi";
            else if (hedefTip == 4) ad = (harita[hedefKY][hedefKX] == T_OTO_E) ? "EBar Otomati (1 Kurus)" : "Lsoda Otomati (1 Kurus)";
            else if (hedefTip == 5) ad = "Balon: " + std::to_string(balonlar[hedefId].deger);
            else if (hedefTip == 6) ad = elBalon >= 0 ? "Makine (balonu ver)" : "Makine";
            else if (hedefTip == 7) ad = "Kantinci Teyze (alisveris)";
            else ad = esAd[esyalar[hedefId].tip];
            yaz(ad, SW / 2 + 12, SH / 2 + 8, 1.5f);
        }

        // ---- akilli tahta ----
        if (tahta.acik) {
            auto yazR = [&](const std::string& t, float X, float Y, float k, int R, int G, int B) {
                SDL_SetRenderDrawColor(r, (Uint8)R, (Uint8)G, (Uint8)B, 255);
                yaz(t, X, Y, k);
            };
            doldur(0, 0, SW, SH, 0, 0, 0, 170);
            doldur(20, 20, 760, 560, 95, 62, 36);                 // cerceve
            doldur(32, 32, 736, 536, 38, 88, 72);                 // tahta
            yazR("AKILLI TAHTA - DEFTER " + std::to_string(tahta.defterNo + 1), 52, 48, 3, 235, 235, 220);
            // solda 3 slot: dogru cevap = yesil kutu
            for (int i = 0; i < 3; i++) {
                float sy = 130.f + i * 140.f;
                bool yes = tahta.yesil[i], simdi = (i == tahta.soru);
                doldur(48, sy - 4, 136, 120, simdi ? 255 : 30, simdi ? 220 : 60, simdi ? 80 : 50);
                doldur(52, sy, 128, 112, yes ? 60 : 150, yes ? 200 : 150, yes ? 90 : 150);
                yazR(std::to_string(i + 1), 108, sy + 40, 4, 25, 45, 25);
            }
            // sagda ekran: islem
            doldur(200, 110, 556, 240, 30, 30, 40);
            doldur(208, 118, 540, 224, 235, 240, 235);
            yazR("Soru " + std::to_string(std::min(tahta.soru + 1, 3)) + "/3", 220, 128, 2, 110, 110, 120);
            float qy = 118 + 92;
            if (tahta.pi) {
                float x0 = 208 + (540 - 9 * 40) / 2.0f;
                yazR("2 + ", x0, qy, 5, 30, 40, 50);
                float px0 = x0 + 160;                              // pi sayisi: elle cizilen harf
                doldur(px0 + 3, qy + 8, 34, 7, 30, 40, 50);
                doldur(px0 + 10, qy + 15, 7, 24, 30, 40, 50);
                doldur(px0 + 24, qy + 15, 7, 20, 30, 40, 50);
                doldur(px0 + 24, qy + 33, 12, 6, 30, 40, 50);
                yazR(" = ?", x0 + 200, qy, 5, 30, 40, 50);
            } else {
                float tw = (float)tahta.metin.size() * 40.0f;
                yazR(tahta.metin, 208 + (540 - tw) / 2.0f, qy, 5, 30, 40, 50);
            }
            // altta 3 sik
            for (int i = 0; i < 3; i++) {
                SDL_FRect b = tahtaSecRect(i);
                bool el = tahta.elendi[i];
                bevel(b.x, b.y, b.w, b.h, el ? 120 : 205, el ? 120 : 190, el ? 120 : 120);
                yazR(std::to_string(i + 1), b.x + 10, b.y + 10, 2, 90, 70, 30);
                float n = (float)tahta.sec[i].size();
                float k = std::min(4.0f, 148.0f / (n * 8.0f));
                yazR(tahta.sec[i], b.x + (b.w - n * 8.0f * k) / 2.0f, b.y + (b.h - 8.0f * k) / 2.0f + 6, k,
                     el ? 70 : 30, el ? 70 : 30, el ? 70 : 40);
            }
            if (!tahta.mesaj.empty()) {
                bool kotu = tahta.mesaj.rfind("Yanl", 0) == 0;
                yazR(tahta.mesaj, 232, 512, 2, kotu ? 255 : 170, kotu ? 150 : 255, kotu ? 120 : 170);
            }
            yazR("Dokun veya 1-2-3 tusu", 52, 548, 1.5f, 200, 220, 200);
        }

        // ---- nobetci ogrenci: refleks testi ----
        if (nbMod == 2) {
            auto yazN = [&](const std::string& t, float X, float Y, float k, int R, int G, int B) {
                SDL_SetRenderDrawColor(r, (Uint8)R, (Uint8)G, (Uint8)B, 255);
                yaz(t, X, Y, k);
            };
            doldur(0, 0, SW, SH, 0, 0, 0, 120);
            SDL_FRect P = nbKagitRect();
            doldur(P.x - 6, P.y - 6, P.w + 12, P.h + 12, 70, 70, 80);                    // kagidin golgesi / tahta
            doldur(P.x, P.y, P.w, P.h, 250, 250, 242);                                   // kagit
            doldur(P.x + P.w / 2 - 30, P.y - 14, 60, 22, 150, 150, 160);                 // kelepce
            for (int i = 0; i < 6; i++) doldur(P.x + 24, P.y + 120 + i * 26, P.w - 48, 3, 190, 190, 200);   // cizgiler
            doldur(P.x - 26, P.y + 170, 40, 60, 240, 205, 170);                          // eller (kagidi tutuyor)
            doldur(P.x + P.w - 14, P.y + 170, 40, 60, 240, 205, 170);
            yazN("NOBET LISTESI", P.x + (P.w - 13 * 16.0f) / 2, P.y + 12, 2, 90, 90, 100);
            if (nbFaz == 2) {                                                            // rengin adi kagidin ustunde
                std::string ad = nbHedef == 0 ? "YESIL" : "MAVI";
                float k = 6.0f;
                yazN(ad, P.x + (P.w - (float)ad.size() * 8.0f * k) / 2, P.y + 46, k, 20, 20, 25);
                float oran = (float)std::clamp(1.0 - nbT / nbTepki, 0.0, 1.0);        // kalan sure cubugu
                doldur(P.x + 24, P.y + 108, (P.w - 48) * oran, 6, 220, 60, 60);
            } else if (nbFaz == 1) {
                yazN("Bekle...", P.x + (P.w - 8 * 16.0f) / 2, P.y + 60, 2, 150, 150, 160);
            }
            for (int i = 0; i < 2; i++) {                                                // alt kosede renk kutulari
                SDL_FRect b = nbDugme(i);
                if (i == 0) bevel(b.x, b.y, b.w, b.h, 40, 170, 70); else bevel(b.x, b.y, b.w, b.h, 50, 90, 230);
            }
            yazN("Hak: " + std::to_string(std::max(0, NOBET_HAK - nbBasarisiz)) + "/" + std::to_string(NOBET_HAK), 12, 12, 2, 255, 255, 255);
            if (!nbMesaj.empty()) yazN(nbMesaj, (SW - (float)nbMesaj.size() * 16.0f) / 2, P.y + P.h + 14, 2, 255, 190, 140);
            yazN("Dokun / A-D / Sol-Sag", 12, SH - 22, 1.5f, 200, 220, 200);
        }

        // ---- kantinci teyze: alisveris ekrani ----
        if (kantinAcik) {
            auto yazK = [&](const std::string& t, float X, float Y, float k, int R, int G, int B) {
                SDL_SetRenderDrawColor(r, (Uint8)R, (Uint8)G, (Uint8)B, 255);
                yaz(t, X, Y, k);
            };
            doldur(0, 0, SW, SH, 0, 0, 0, 175);
            doldur(40, 14, 720, 64, 120, 80, 40);
            yazK("KANTIN", 60, 30, 4, 255, 235, 200);
            yazK("MS: " + std::to_string(msPuan), SW - 230, 36, 3, 255, 230, 120);
            for (int i = 0; i < 5; i++) {
                SDL_FRect b = kantinSatir(i);
                int tip = KANTIN_URUN[i], fiyat = KANTIN_FIYAT[i];
                bool yeter = msPuan >= fiyat;
                if (yeter) bevel(b.x, b.y, b.w, b.h, 55, 120, 75); else bevel(b.x, b.y, b.w, b.h, 80, 80, 88);
                doldur(b.x + 8, b.y + 8, 60, 60, 20, 20, 30, 170);
                ikon(tip, b.x + 38, b.y + 38, 52);
                yazK(std::to_string(i + 1) + ". " + esAd[tip], b.x + 84, b.y + 10, 2.5f, 255, 255, 255);
                yazK(esAcik[tip], b.x + 84, b.y + 46, 1.5f, 215, 235, 215);
                yazK(std::to_string(fiyat) + " MS", b.x + b.w - 130, b.y + 26, 3, yeter ? 255 : 180, yeter ? 230 : 180, yeter ? 120 : 180);
            }
            SDL_FRect g = kantinGeri();
            bevel(g.x, g.y, g.w, g.h, 170, 60, 60);
            yazK("GERI", g.x + 52, g.y + 16, 3, 255, 255, 255);
            if (kantinMesajT > 0 && !kantinMesaj.empty())
                yazK(kantinMesaj, (SW - (float)kantinMesaj.size() * 16.0f) / 2, 84, 2, 255, 220, 140);
            yazK("Dokun / 1-5 satin al, B geri", 12, SH - 22, 1.5f, 200, 220, 200);
        }

        // ---- kanca: cekilirken ekranda ip ----
        if (kanca.var) {
            SDL_SetRenderDrawColor(r, 185, 135, 70, 255);
            for (int o = -2; o <= 2; o++) SDL_RenderLine(r, SW / 2.0f + o * 3, SH - 70.0f, SW / 2.0f + o, SH / 2.0f + 8);
        }

        if (durum != 0) {
            if (durum == 1) doldur(0, 0, SW, SH, 20, 120, 40, 150);
            else doldur(0, 0, SW, SH, 140, 20, 20, 150);
            const char* b = durum == 1 ? "KAZANDIN!" : "YAKALANDIN!";
            yaz(b, SW / 2.0f - (float)std::strlen(b) * 16.0f, SH / 2.0f - 30, 4);
            const char* a = "R veya dokun = yeniden";
            yaz(a, SW / 2.0f - (float)std::strlen(a) * 8.0f, SH / 2.0f + 20, 2);
        }

        if (taniSure > 0) {
            float yy = 6;
            doldur(0, 0, SW, 16.0f * (float)tani.size() + 10, 0, 0, 0, 170);
            for (auto& l : tani) { yaz(l, 8, yy, 1.5f); yy += 16; }
        }
        SDL_RenderPresent(r);
        SDL_Delay(4);
    }

    for (auto& m : img) if (m.t) SDL_DestroyTexture(m.t);
#ifdef HAS_MIX
    if (gitTrack) MIX_StopTrack(gitTrack, 0);
    if (klasikTrack) MIX_StopTrack(klasikTrack, 0);
    if (defterTrack) MIX_StopTrack(defterTrack, 0);
    if (vatTrack) MIX_StopTrack(vatTrack, 0);
    if (tehlikeTrack) MIX_StopTrack(tehlikeTrack, 0);
    if (dukkanTrack) MIX_StopTrack(dukkanTrack, 0);
    if (mikser) MIX_DestroyMixer(mikser);
    MIX_Quit();
#endif
    if (sesAkis) SDL_DestroyAudioStream(sesAkis);
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(r);
    SDL_DestroyWindow(pencere);
    SDL_Quit();
    return 0;
}
