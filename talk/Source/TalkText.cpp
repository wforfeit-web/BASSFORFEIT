#include "TalkEngine.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>

namespace talk
{
//==============================================================================
// Phoneme table. Formant targets follow classic published measurements of American English
// (Peterson & Barney 1952; Klatt 1980), tuned for a clear, slightly robotic voice.
namespace
{
using K = Kind;
const PhonemeDef kPhonemes[numPhonemes] = {
    //  name  kind          vcd    f1   f2    f3     e1   e2    e3     b1  b2   b3    fricF  fricQ af     dur  closure
    { "AA", K::Vowel,     true,  700, 1220, 2600,    0,    0,    0,   80,  90, 150,     0, 1.0f, 0.0f, 150,  0 },
    { "AE", K::Vowel,     true,  660, 1720, 2410,    0,    0,    0,   80,  90, 150,     0, 1.0f, 0.0f, 150,  0 },
    { "AH", K::Vowel,     true,  620, 1220, 2550,    0,    0,    0,   80,  90, 150,     0, 1.0f, 0.0f, 100,  0 },
    { "AO", K::Vowel,     true,  600,  990, 2570,    0,    0,    0,   80,  90, 150,     0, 1.0f, 0.0f, 160,  0 },
    { "AW", K::Diphthong, true,  680, 1200, 2550,  400,  900, 2350,  80,  90, 150,     0, 1.0f, 0.0f, 200,  0 },
    { "AY", K::Diphthong, true,  680, 1200, 2550,  380, 1950, 2550,  80,  90, 150,     0, 1.0f, 0.0f, 200,  0 },
    { "B",  K::Stop,      true,  200, 1000, 2150,    0,    0,    0,   60, 100, 150,  1500, 0.6f, 0.25f, 75, 60 },
    { "CH", K::Affricate, false, 350, 1800, 2820,    0,    0,    0,   60, 100, 150,  3200, 1.6f, 0.75f,110, 50 },
    { "D",  K::Stop,      true,  200, 1700, 2600,    0,    0,    0,   60, 100, 150,  3800, 1.2f, 0.40f, 65, 50 },
    { "DH", K::Fricative, true,  320, 1290, 2540,    0,    0,    0,   60, 100, 150,  4500, 0.8f, 0.03f, 55,  0 },
    { "EH", K::Vowel,     true,  530, 1680, 2500,    0,    0,    0,   70,  90, 150,     0, 1.0f, 0.0f, 110,  0 },
    { "ER", K::Vowel,     true,  470, 1270, 1540,    0,    0,    0,   70, 100, 110,     0, 1.0f, 0.0f, 150,  0 },
    { "EY", K::Diphthong, true,  500, 1750, 2520,  330, 2200, 2850,  70,  90, 150,     0, 1.0f, 0.0f, 170,  0 },
    { "F",  K::Fricative, false, 340, 1100, 2080,    0,    0,    0,   60, 100, 150,  5000, 0.6f, 0.13f,100,  0 },
    { "G",  K::Stop,      true,  200, 1900, 2500,    0,    0,    0,   60, 100, 150,  2300, 2.0f, 0.45f, 70, 55 },
    { "HH", K::Aspirate,  false,   0,    0,    0,    0,    0,    0,  300, 300, 300,     0, 1.0f, 0.0f,  65,  0 },
    { "IH", K::Vowel,     true,  400, 1800, 2570,    0,    0,    0,   60,  90, 150,     0, 1.0f, 0.0f,  90,  0 },
    { "IY", K::Vowel,     true,  290, 2250, 3000,    0,    0,    0,   60,  90, 150,     0, 1.0f, 0.0f, 140,  0 },
    { "JH", K::Affricate, true,  260, 1800, 2820,    0,    0,    0,   60, 100, 150,  3000, 1.6f, 0.50f, 90, 45 },
    { "K",  K::Stop,      false, 250, 1900, 2500,    0,    0,    0,   60, 100, 150,  2300, 2.0f, 0.80f,100, 65 },
    { "L",  K::Liquid,    true,  330,  900, 2880,    0,    0,    0,   60, 100, 200,     0, 1.0f, 0.0f,  80,  0 },
    { "M",  K::Nasal,     true,  250, 1100, 2150,    0,    0,    0,   60, 250, 300,   800, 1.0f, 0.0f,  90,  0 },
    { "N",  K::Nasal,     true,  250, 1700, 2600,    0,    0,    0,   60, 250, 300,  1500, 1.0f, 0.0f,  80,  0 },
    { "NG", K::Nasal,     true,  250, 2200, 2700,    0,    0,    0,   60, 250, 300,  2500, 1.0f, 0.0f,  95,  0 },
    { "OW", K::Diphthong, true,  540, 1100, 2300,  380,  880, 2300,  80,  90, 150,     0, 1.0f, 0.0f, 180,  0 },
    { "OY", K::Diphthong, true,  550,  960, 2400,  360, 1820, 2450,  80,  90, 150,     0, 1.0f, 0.0f, 220,  0 },
    { "P",  K::Stop,      false, 200, 1000, 2150,    0,    0,    0,   60, 100, 150,  1500, 0.6f, 0.50f, 95, 65 },
    { "R",  K::Liquid,    true,  310, 1060, 1380,    0,    0,    0,   70, 100, 120,     0, 1.0f, 0.0f,  75,  0 },
    { "S",  K::Fricative, false, 320, 1390, 2530,    0,    0,    0,   60, 100, 150,  6000, 2.2f, 0.85f,130,  0 },
    { "SH", K::Fricative, false, 300, 1840, 2750,    0,    0,    0,   60, 100, 150,  2900, 2.0f, 0.80f,120,  0 },
    { "T",  K::Stop,      false, 200, 1700, 2600,    0,    0,    0,   60, 100, 150,  4200, 1.3f, 0.80f, 90, 55 },
    { "TH", K::Fricative, false, 320, 1290, 2540,    0,    0,    0,   60, 100, 150,  5500, 0.6f, 0.10f, 90,  0 },
    { "UH", K::Vowel,     true,  450, 1100, 2350,    0,    0,    0,   70,  90, 150,     0, 1.0f, 0.0f, 100,  0 },
    { "UW", K::Vowel,     true,  320, 1000, 2250,    0,    0,    0,   65,  90, 150,     0, 1.0f, 0.0f, 150,  0 },
    { "V",  K::Fricative, true,  340, 1100, 2080,    0,    0,    0,   60, 100, 150,  4500, 0.6f, 0.04f, 65,  0 },
    { "W",  K::Glide,     true,  290,  610, 2150,    0,    0,    0,   60,  90, 150,     0, 1.0f, 0.0f,  80,  0 },
    { "Y",  K::Glide,     true,  260, 2070, 3020,    0,    0,    0,   60,  90, 150,     0, 1.0f, 0.0f,  65,  0 },
    { "Z",  K::Fricative, true,  320, 1390, 2530,    0,    0,    0,   60, 100, 150,  6000, 2.2f, 0.55f, 85,  0 },
    { "ZH", K::Fricative, true,  300, 1840, 2750,    0,    0,    0,   60, 100, 150,  2900, 2.0f, 0.50f, 80,  0 },
    { "PAUSE", K::Pause,  false,   0,    0,    0,    0,    0,    0,  100, 100, 100,     0, 1.0f, 0.0f, 100,  0 },
};

bool isVowelId (int id)
{
    const auto k = kPhonemes[id].kind;
    return k == Kind::Vowel || k == Kind::Diphthong;
}
} // namespace

const PhonemeDef& phoneme (int id) { return kPhonemes[(id >= 0 && id < numPhonemes) ? id : PAUSE]; }

int phonemeId (const std::string& name)
{
    for (int i = 0; i < numPhonemes; ++i)
        if (name == kPhonemes[i].name) return i;
    return -1;
}

//==============================================================================
void Dictionary::load (const std::string& text)
{
    entries.clear();
    entries.reserve (140000);
    std::istringstream in (text);
    std::string line;
    while (std::getline (in, line))
    {
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        const auto space = line.find (' ');
        if (space == std::string::npos) continue;
        std::string word = line.substr (0, space);
        if (word.find ('(') != std::string::npos) continue;   // keep the first pronunciation only
        for (auto& c : word) c = (char) std::toupper ((unsigned char) c);
        auto pron = line.substr (space + 1);
        const auto hash = pron.find ('#');
        if (hash != std::string::npos) pron = pron.substr (0, hash);
        while (! pron.empty() && (pron.back() == ' ' || pron.back() == '\r')) pron.pop_back();
        while (! pron.empty() && pron.front() == ' ') pron.erase (pron.begin());
        for (auto& c : pron) c = (char) std::toupper ((unsigned char) c);
        if (! pron.empty()) entries.emplace (std::move (word), std::move (pron));
    }
}

const std::string* Dictionary::find (const std::string& upperWord) const
{
    const auto it = entries.find (upperWord);
    return it == entries.end() ? nullptr : &it->second;
}

//==============================================================================
namespace
{
struct PhonStress { int id; int stress; };
using Pron = std::vector<PhonStress>;

// Parses "HH AH0 L OW1"; "AX" means an unstressed AH
Pron parsePron (const std::string& s)
{
    Pron out;
    std::istringstream in (s);
    std::string tok;
    while (in >> tok)
    {
        int stress = -1;
        if (! tok.empty() && std::isdigit ((unsigned char) tok.back()))
        {
            stress = tok.back() - '0';
            tok.pop_back();
        }
        if (tok == "AX") { tok = "AH"; stress = 0; }
        const int id = phonemeId (tok);
        if (id < 0 || id == PAUSE) continue;
        if (isVowelId (id) && stress < 0) stress = -1;   // filled in later
        out.push_back ({ id, isVowelId (id) ? stress : 0 });
    }
    return out;
}

// Rule output has no stress marks: stress the first full vowel, reduce the rest
void assignStress (Pron& p)
{
    bool first = true;
    for (auto& ps : p)
    {
        if (! isVowelId (ps.id) || ps.stress >= 0) { if (isVowelId (ps.id) && ps.stress > 0) first = false; continue; }
        ps.stress = first ? 1 : 0;
        first = false;
    }
}

//==============================================================================
// Letter-to-sound rules for words that are not in the dictionary. These follow the style of the
// public-domain Naval Research Laboratory rules (Elovitz et al., 1976). Context symbols:
//   ' ' word edge   '#' one or more vowels   ':' zero or more consonants   '^' one consonant
//   '.' voiced consonant   '+' E, I or Y   '%' suffix (ER E ES ED ING ELY)
//   '&' sibilant   '@' T S R D L Z N J TH CH SH
struct Rule { const char* left; const char* match; const char* right; const char* out; };

const Rule kRules[] = {
    // A
    { " ", "A", " ", "AX" }, { " ", "ARE", " ", "AA R" }, { " ", "AR", "O", "AX R" }, { "", "AR", "#", "EH R" },
    { " ^", "AS", "#", "EY S" }, { "", "A", "WA", "AX" }, { "", "AW", "", "AO" }, { " :", "ANY", "", "EH N IY" },
    { "", "A", "^+#", "EY" }, { "#:", "ALLY", "", "AX L IY" }, { " ", "AL", "#", "AX L" }, { "", "AGAIN", "", "AX G EH N" },
    { "#:", "AG", "E", "IH JH" }, { "", "A", "^+:#", "AE" }, { " :", "A", "^+ ", "EY" }, { "", "A", "^%", "EY" },
    { " ", "ARR", "", "AX R" }, { "", "ARR", "", "AE R" }, { " :", "AR", " ", "AA R" }, { "", "AR", " ", "ER" },
    { "", "AR", "", "AA R" }, { "", "AIR", "", "EH R" }, { "", "AI", "", "EY" }, { "", "AY", "", "EY" },
    { "", "AU", "", "AO" }, { "#:", "AL", " ", "AX L" }, { "#:", "ALS", " ", "AX L Z" }, { "", "ALK", "", "AO K" },
    { "", "AL", "^", "AO L" }, { " :", "ABLE", "", "EY B AX L" }, { "", "ABLE", "", "AX B AX L" },
    { "", "ANG", "+", "EY N JH" }, { "", "A", "", "AE" },
    // B
    { " ", "BE", "^#", "B IH" }, { "", "BEING", "", "B IY IH NG" }, { " ", "BOTH", " ", "B OW TH" },
    { " ", "BUS", "#", "B IH Z" }, { "", "BUIL", "", "B IH L" }, { "", "BB", "", "B" }, { "", "B", "", "B" },
    // C
    { " ", "CH", "^", "K" }, { "^E", "CH", "", "K" }, { "", "CH", "", "CH" }, { " S", "CI", "#", "S AY" },
    { "", "CI", "A", "SH" }, { "", "CI", "O", "SH" }, { "", "CI", "EN", "SH" }, { "", "C", "+", "S" },
    { "", "CK", "", "K" }, { "", "COM", "%", "K AH M" }, { "", "CC", "+", "K S" }, { "", "CC", "", "K" },
    { "", "C", "", "K" },
    // D
    { "#:", "DED", " ", "D IH D" }, { ".E", "D", " ", "D" }, { "#^:E", "D", " ", "T" }, { " ", "DE", "^#", "D IH" },
    { " ", "DO", " ", "D UW" }, { " ", "DOES", "", "D AH Z" }, { " ", "DOING", "", "D UW IH NG" },
    { " ", "DOW", "", "D AW" }, { "", "DU", "A", "JH UW" }, { "", "DD", "", "D" }, { "", "DGE", "", "JH" },
    { "", "D", "", "D" },
    // E
    { "#:", "E", " ", "" }, { "'^:", "E", " ", "" }, { " :", "E", " ", "IY" }, { "#", "ED", " ", "D" },
    { "#:", "E", "D ", "" }, { "", "EV", "ER", "EH V" }, { "", "E", "^%", "IY" }, { "", "ERI", "#", "IY R IY" },
    { "", "ERI", "", "EH R IH" }, { "#:", "ER", "#", "ER" }, { "", "ER", "#", "EH R" }, { "", "ER", "", "ER" },
    { " ", "EVEN", "", "IY V EH N" }, { "#:", "E", "W", "" }, { "@", "EW", "", "UW" }, { "", "EW", "", "Y UW" },
    { "", "E", "O", "IY" }, { "#:&", "ES", " ", "IH Z" }, { "#:", "E", "S ", "" }, { "#:", "ELY", " ", "L IY" },
    { "#:", "EMENT", "", "M EH N T" }, { "", "EFUL", "", "F UH L" }, { "", "EE", "", "IY" }, { "", "EARN", "", "ER N" },
    { " ", "EAR", "^", "ER" }, { "", "EAD", "", "EH D" }, { "#:", "EA", " ", "IY AX" }, { "", "EA", "SU", "EH" },
    { "", "EA", "", "IY" }, { "", "EIGH", "", "EY" }, { "", "EI", "", "IY" }, { " ", "EYE", "", "AY" },
    { "", "EY", "", "IY" }, { "", "EU", "", "Y UW" }, { "", "E", "", "EH" },
    // F
    { "", "FUL", "", "F UH L" }, { "", "FF", "", "F" }, { "", "F", "", "F" },
    // G
    { "", "GIV", "", "G IH V" }, { " ", "G", "I^", "G" }, { "", "GE", "T", "G EH" }, { "SU", "GGES", "", "G JH EH S" },
    { "", "GG", "", "G" }, { " B#", "G", "", "G" }, { "", "G", "+", "JH" }, { "", "GREAT", "", "G R EY T" },
    { "#", "GH", "", "" }, { "", "G", "", "G" },
    // H
    { " ", "HAV", "", "HH AE V" }, { " ", "HERE", "", "HH IY R" }, { " ", "HOUR", "", "AW ER" }, { "", "HOW", "", "HH AW" },
    { "", "H", "#", "HH" }, { "", "H", "", "" },
    // I
    { " ", "IN", "", "IH N" }, { " ", "I", " ", "AY" }, { "", "IN", "D", "AY N" }, { "", "IER", "", "IY ER" },
    { "#:R", "IED", "", "IY D" }, { "", "IED", " ", "AY D" }, { "", "IEN", "", "IY EH N" }, { "", "IE", "T", "AY EH" },
    { " :", "I", "%", "AY" }, { "", "I", "%", "IY" }, { "", "IE", "", "IY" }, { "", "I", "^+:#", "IH" },
    { "", "IR", "#", "AY R" }, { "", "IZ", "%", "AY Z" }, { "", "IS", "%", "AY Z" }, { "", "I", "D%", "AY" },
    { "+^", "I", "^+", "IH" }, { "", "I", "T%", "AY" }, { "#^:", "I", "^+", "IH" }, { "", "IR", "", "ER" },
    { "", "IGH", "", "AY" }, { "", "ILD", "", "AY L D" }, { "", "IGN", " ", "AY N" }, { "", "IGN", "^", "AY N" },
    { "", "IGN", "%", "AY N" }, { "", "IQUE", "", "IY K" }, { "", "I", "", "IH" },
    // J K L M
    { "", "J", "", "JH" }, { " ", "K", "N", "" }, { "", "K", "", "K" },
    { "", "LO", "C#", "L OW" }, { "L", "L", "", "" }, { "#^:", "L", "%", "AX L" }, { "", "LEAD", "", "L IY D" },
    { "", "L", "", "L" }, { "", "MOV", "", "M UW V" }, { "", "MM", "", "M" }, { "", "M", "", "M" },
    // N
    { "E", "NG", "+", "N JH" }, { "", "NG", "R", "NG G" }, { "", "NG", "#", "NG G" }, { "", "NGL", "%", "NG G AX L" },
    { "", "NG", "", "NG" }, { "", "NK", "", "NG K" }, { " ", "NOW", " ", "N AW" }, { "", "NN", "", "N" },
    { "", "N", "", "N" },
    // O
    { "", "OF", " ", "AX V" }, { "", "OROUGH", "", "ER OW" }, { "#:", "OR", " ", "ER" }, { "#:", "ORS", " ", "ER Z" },
    { "", "OR", "", "AO R" }, { " ", "ONE", "", "W AH N" }, { "", "OW", "", "OW" }, { " ", "OVER", "", "OW V ER" },
    { "", "OV", "", "AH V" }, { "", "O", "^%", "OW" }, { "", "O", "^EN", "OW" }, { "", "O", "^I#", "OW" },
    { "", "OL", "D", "OW L" }, { "", "OUGHT", "", "AO T" }, { "", "OUGH", "", "AH F" }, { " ", "OU", "", "AW" },
    { "H", "OU", "S#", "AW" }, { "", "OUS", "", "AX S" }, { "", "OUR", "", "AO R" }, { "", "OULD", "", "UH D" },
    { "^", "OU", "^L", "AH" }, { "", "OUP", "", "UW P" }, { "", "OU", "", "AW" }, { "", "OY", "", "OY" },
    { "", "OING", "", "OW IH NG" }, { "", "OI", "", "OY" }, { "", "OOR", "", "AO R" }, { "", "OOK", "", "UH K" },
    { "", "OOD", "", "UH D" }, { "", "OO", "", "UW" }, { "", "O", "E", "OW" }, { "", "O", " ", "OW" },
    { "", "OA", "", "OW" }, { " ", "ONLY", "", "OW N L IY" }, { " ", "ONCE", "", "W AH N S" }, { "", "ON'T", "", "OW N T" },
    { "C", "O", "N", "AA" }, { "", "O", "NG", "AO" }, { " :^", "O", "N", "AH" }, { "I", "ON", "", "AX N" },
    { "#:", "ON", " ", "AX N" }, { "#^", "ON", "", "AX N" }, { "", "O", "ST ", "OW" }, { "", "OF", "^", "AO F" },
    { "", "OTHER", "", "AH DH ER" }, { "", "OSS", " ", "AO S" }, { "#:^", "OM", "", "AH M" }, { "", "O", "", "AA" },
    // P Q R
    { "", "PH", "", "F" }, { "", "PEOP", "", "P IY P" }, { "", "POW", "", "P AW" }, { "", "PUT", " ", "P UH T" },
    { "", "PP", "", "P" }, { "", "P", "", "P" },
    { "", "QUAR", "", "K W AO R" }, { "", "QU", "", "K W" }, { "", "Q", "", "K" },
    { " ", "RE", "^#", "R IY" }, { "", "RR", "", "R" }, { "", "R", "", "R" },
    // S
    { "", "SH", "", "SH" }, { "#", "SION", "", "ZH AX N" }, { "", "SOME", "", "S AH M" }, { "#", "SUR", "#", "ZH ER" },
    { "", "SUR", "#", "SH ER" }, { "#", "SU", "#", "ZH UW" }, { "#", "SSU", "#", "SH UW" }, { "#", "SED", " ", "Z D" },
    { "#", "S", "#", "Z" }, { "", "SAID", "", "S EH D" }, { "^", "SION", "", "SH AX N" }, { "", "S", "S", "" },
    { ".", "S", " ", "Z" }, { "#:.E", "S", " ", "Z" }, { "#:^##", "S", " ", "Z" }, { "#:^#", "S", " ", "S" },
    { "U", "S", " ", "S" }, { " :#", "S", " ", "Z" }, { " ", "SCH", "", "S K" }, { "", "S", "C+", "" },
    { "#", "SM", "", "Z M" }, { "#", "SN", "'", "Z AX N" }, { "", "S", "", "S" },
    // T
    { " ", "THE", " ", "DH AX" }, { "", "TO", " ", "T UW" }, { "", "THAT", " ", "DH AE T" }, { " ", "THIS", " ", "DH IH S" },
    { " ", "THEY", "", "DH EY" }, { " ", "THERE", "", "DH EH R" }, { "", "THER", "", "DH ER" }, { "", "THEIR", "", "DH EH R" },
    { " ", "THAN", " ", "DH AE N" }, { " ", "THEM", " ", "DH EH M" }, { "", "THESE", " ", "DH IY Z" },
    { " ", "THEN", "", "DH EH N" }, { "", "THROUGH", "", "TH R UW" }, { "", "THOSE", "", "DH OW Z" },
    { "", "THOUGH", " ", "DH OW" }, { " ", "THUS", "", "DH AH S" }, { "", "TH", "", "TH" }, { "#:", "TED", " ", "T IH D" },
    { "S", "TI", "#N", "CH" }, { "", "TI", "O", "SH" }, { "", "TI", "A", "SH" }, { "", "TIEN", "", "SH AX N" },
    { "", "TUR", "#", "CH ER" }, { "", "TU", "A", "CH UW" }, { " ", "TWO", "", "T UW" }, { "", "TT", "", "T" },
    { "", "T", "", "T" },
    // U
    { " ", "UN", "I", "Y UW N" }, { " ", "UN", "", "AH N" }, { " ", "UPON", "", "AX P AO N" }, { "@", "UR", "#", "UH R" },
    { "", "UR", "#", "Y UH R" }, { "", "UR", "^", "ER" }, { "", "U", "^ ", "AH" }, { "", "U", "^^", "AH" },
    { "", "UY", "", "AY" }, { " G", "U", "#", "" }, { "G", "U", "%", "" }, { "G", "U", "#", "W" },
    { "#N", "U", "", "Y UW" }, { "@", "U", "", "UW" }, { "", "U", "", "Y UW" },
    // V W X Y Z
    { "", "VIEW", "", "V Y UW" }, { "", "V", "", "V" },
    { " ", "WERE", "", "W ER" }, { "", "WA", "S", "W AA" }, { "", "WA", "T", "W AA" }, { "", "WHERE", "", "W EH R" },
    { "", "WHAT", "", "W AH T" }, { "", "WHOL", "", "HH OW L" }, { "", "WHO", "", "HH UW" }, { "", "WH", "", "W" },
    { "", "WAR", "", "W AO R" }, { "", "WOR", "^", "W ER" }, { "", "WR", "", "R" }, { "", "W", "", "W" },
    { " ", "X", "", "Z" }, { "", "X", "", "K S" },
    { "", "YOUNG", "", "Y AH NG" }, { " ", "YOU", "", "Y UW" }, { " ", "YES", "", "Y EH S" }, { " ", "Y", "", "Y" },
    { "#^:", "Y", " ", "IY" }, { "#^:", "Y", "I", "IY" }, { " :", "Y", " ", "AY" }, { " :", "Y", "#", "AY" },
    { " :", "Y", "^+:#", "IH" }, { " :", "Y", "^#", "AY" }, { "", "Y", "", "IH" },
    { "", "ZZ", "", "Z" }, { "", "Z", "", "Z" },
};

bool isVowelLetter (char c) { return c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U'; }
bool isLetter (char c) { return c >= 'A' && c <= 'Z'; }
bool isConsonant (char c) { return isLetter (c) && ! isVowelLetter (c); }
bool isVoicedCons (char c) { return std::strchr ("BDVGJLMNRWZ", c) != nullptr && c != 0; }
bool isFront (char c) { return c == 'E' || c == 'I' || c == 'Y'; }

// Matches a right-context pattern starting at position i of w (w is padded with spaces)
bool matchRight (const std::string& w, int i, const char* pat)
{
    for (const char* p = pat; *p; ++p)
    {
        const char c = i < (int) w.size() ? w[(size_t) i] : ' ';
        switch (*p)
        {
            case ' ': if (isLetter (c)) return false; ++i; break;
            case '#': if (! isVowelLetter (c)) return false; while (i < (int) w.size() && isVowelLetter (w[(size_t) i])) ++i; break;
            case ':': while (i < (int) w.size() && isConsonant (w[(size_t) i])) ++i; break;
            case '^': if (! isConsonant (c)) return false; ++i; break;
            case '.': if (! isVoicedCons (c)) return false; ++i; break;
            case '+': if (! isFront (c)) return false; ++i; break;
            case '&':
                if (w.compare ((size_t) i, 2, "CH") == 0 || w.compare ((size_t) i, 2, "SH") == 0) { i += 2; break; }
                if (std::strchr ("SCGZXJ", c) == nullptr || c == 0) { return false; } ++i; break;
            case '@':
                if (w.compare ((size_t) i, 2, "TH") == 0 || w.compare ((size_t) i, 2, "CH") == 0 || w.compare ((size_t) i, 2, "SH") == 0) { i += 2; break; }
                if (std::strchr ("TSRDLZNJ", c) == nullptr || c == 0) { return false; } ++i; break;
            case '%':
            {
                static const char* const suffixes[] = { "ING", "ELY", "ER", "ES", "ED", "E" };
                bool found = false;
                for (auto* s : suffixes)
                    if (w.compare ((size_t) i, std::strlen (s), s) == 0) { i += (int) std::strlen (s); found = true; break; }
                if (! found) return false;
                break;
            }
            default: if (c != *p) return false; ++i; break;
        }
    }
    return true;
}

// Matches a left-context pattern that ends just before position i
bool matchLeft (const std::string& w, int i, const char* pat)
{
    int j = i - 1;
    for (int k = (int) std::strlen (pat) - 1; k >= 0; --k)
    {
        const char c = j >= 0 ? w[(size_t) j] : ' ';
        switch (pat[k])
        {
            case ' ': if (isLetter (c)) return false; --j; break;
            case '#': if (! isVowelLetter (c)) return false; while (j >= 0 && isVowelLetter (w[(size_t) j])) --j; break;
            case ':': while (j >= 0 && isConsonant (w[(size_t) j])) --j; break;
            case '^': if (! isConsonant (c)) return false; --j; break;
            case '.': if (! isVoicedCons (c)) return false; --j; break;
            case '+': if (! isFront (c)) return false; --j; break;
            case '&':
                if (j >= 1 && (w.compare ((size_t) j - 1, 2, "CH") == 0 || w.compare ((size_t) j - 1, 2, "SH") == 0)) { j -= 2; break; }
                if (std::strchr ("SCGZXJ", c) == nullptr || c == 0) { return false; } --j; break;
            case '@':
                if (j >= 1 && (w.compare ((size_t) j - 1, 2, "TH") == 0 || w.compare ((size_t) j - 1, 2, "CH") == 0 || w.compare ((size_t) j - 1, 2, "SH") == 0)) { j -= 2; break; }
                if (std::strchr ("TSRDLZNJ", c) == nullptr || c == 0) { return false; } --j; break;
            default: if (c != pat[k]) return false; --j; break;
        }
    }
    return true;
}

std::string lettersToSound (const std::string& upperWord)
{
    const std::string w = " " + upperWord + " ";
    std::string out;
    int i = 1;
    while (i < (int) w.size() - 1)
    {
        bool matched = false;
        for (const auto& r : kRules)
        {
            if (r.match[0] != w[(size_t) i]) continue;
            const int len = (int) std::strlen (r.match);
            if (w.compare ((size_t) i, (size_t) len, r.match) != 0) continue;
            if (! matchLeft (w, i, r.left) || ! matchRight (w, i + len, r.right)) continue;
            if (*r.out) { if (! out.empty()) out += ' '; out += r.out; }
            i += len;
            matched = true;
            break;
        }
        if (! matched) ++i;   // apostrophes and anything else are silent
    }
    return out;
}

//==============================================================================
const char* const kOnes[] = { "zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine",
                              "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen",
                              "seventeen", "eighteen", "nineteen" };
const char* const kTens[] = { "", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety" };

void numberWords (long long n, std::vector<std::string>& out)
{
    if (n < 0) { out.push_back ("minus"); n = -n; }
    if (n < 20) { out.push_back (kOnes[n]); return; }
    if (n < 100)
    {
        out.push_back (kTens[n / 10]);
        if (n % 10) out.push_back (kOnes[n % 10]);
        return;
    }
    struct Scale { long long value; const char* name; };
    static const Scale scales[] = { { 1000000000LL, "billion" }, { 1000000LL, "million" }, { 1000LL, "thousand" }, { 100LL, "hundred" } };
    for (const auto& s : scales)
    {
        if (n >= s.value)
        {
            numberWords (n / s.value, out);
            out.push_back (s.name);
            if (n % s.value) numberWords (n % s.value, out);
            return;
        }
    }
}

bool isVoicelessEnd (const Pron& p)
{
    if (p.empty()) return false;
    const auto& d = phoneme (p.back().id);
    return ! d.voiced;
}

bool isSibilantEnd (const Pron& p)
{
    if (p.empty()) return false;
    const std::string n = phoneme (p.back().id).name;
    return n == "S" || n == "Z" || n == "SH" || n == "ZH" || n == "CH" || n == "JH";
}

void append (Pron& p, const char* phones)
{
    auto extra = parsePron (phones);
    for (auto& e : extra) if (isVowelId (e.id) && e.stress < 0) e.stress = 0;
    p.insert (p.end(), extra.begin(), extra.end());
}

Pron pronounce (const std::string& rawWord, const Dictionary& dict)
{
    if (! rawWord.empty() && rawWord.front() == '[')
    {
        std::string inner = rawWord.substr (1, rawWord.size() - 2);
        for (auto& c : inner) c = (char) std::toupper ((unsigned char) c);
        auto p = parsePron (inner);
        assignStress (p);
        return p;
    }

    std::string w;
    for (char c : rawWord) if (std::isalpha ((unsigned char) c) || c == '\'') w += (char) std::toupper ((unsigned char) c);
    while (! w.empty() && w.front() == '\'') w.erase (w.begin());
    while (! w.empty() && w.back() == '\'') w.pop_back();
    if (w.empty()) return {};

    if (auto* p = dict.find (w)) return parsePron (*p);

    // Common endings on a known word
    auto tryBase = [&] (size_t cut, const std::string& addLetters) -> const std::string*
    {
        if (w.size() <= cut + 1) return nullptr;
        return dict.find (w.substr (0, w.size() - cut) + addLetters);
    };
    auto endsWith = [&] (const char* s) { const size_t n = std::strlen (s); return w.size() > n && w.compare (w.size() - n, n, s) == 0; };

    if (endsWith ("'S"))
        if (auto* b = tryBase (2, "")) { auto p = parsePron (*b); append (p, isSibilantEnd (p) ? "IH0 Z" : (isVoicelessEnd (p) ? "S" : "Z")); return p; }
    if (endsWith ("S"))
        if (auto* b = tryBase (1, "")) { auto p = parsePron (*b); append (p, isSibilantEnd (p) ? "IH0 Z" : (isVoicelessEnd (p) ? "S" : "Z")); return p; }
    if (endsWith ("ING"))
    {
        if (auto* b = tryBase (3, "")) { auto p = parsePron (*b); append (p, "IH0 NG"); return p; }
        if (auto* b = tryBase (3, "E")) { auto p = parsePron (*b); append (p, "IH0 NG"); return p; }
    }
    if (endsWith ("ED"))
    {
        const std::string* b = tryBase (2, "");
        if (b == nullptr) b = tryBase (1, "");
        if (b != nullptr)
        {
            auto p = parsePron (*b);
            const std::string last = p.empty() ? "" : phoneme (p.back().id).name;
            append (p, (last == "T" || last == "D") ? "IH0 D" : (isVoicelessEnd (p) ? "T" : "D"));
            return p;
        }
    }
    if (endsWith ("LY")) if (auto* b = tryBase (2, "")) { auto p = parsePron (*b); append (p, "L IY0"); return p; }
    if (endsWith ("ER")) if (auto* b = tryBase (2, "")) { auto p = parsePron (*b); append (p, "ER0"); return p; }

    auto p = parsePron (lettersToSound (w));
    assignStress (p);
    return p;
}
} // namespace

//==============================================================================
std::unique_ptr<Utterance> buildUtterance (const std::string& text, const Dictionary& dict)
{
    auto u = std::make_unique<Utterance>();

    struct Token { std::string text; std::vector<std::string> spoken; int pauseMs = 0; char boundary = 0; };
    std::vector<Token> tokens;

    // ---- Split into words, numbers and punctuation ----
    size_t i = 0;
    while (i < text.size())
    {
        const unsigned char c = (unsigned char) text[i];
        if (c == '[')
        {
            // Raw phonemes typed directly, e.g. [HH AH0 L OW1]
            const size_t j = text.find (']', i);
            const size_t end = j == std::string::npos ? text.size() : j;
            Token t; t.text = text.substr (i + 1, end - i - 1); t.spoken.push_back ("[" + t.text + "]");
            tokens.push_back (t);
            i = end + 1;
        }
        else if (std::isalpha (c) || c == '\'')
        {
            size_t j = i;
            while (j < text.size() && (std::isalpha ((unsigned char) text[j]) || text[j] == '\'')) ++j;
            Token t; t.text = text.substr (i, j - i); t.spoken.push_back (t.text);
            tokens.push_back (t);
            i = j;
        }
        else if (std::isdigit (c))
        {
            size_t j = i;
            while (j < text.size() && (std::isdigit ((unsigned char) text[j]) || text[j] == ',')) ++j;
            std::string digits;
            for (size_t k = i; k < j; ++k) if (text[k] != ',') digits += text[k];
            Token t; t.text = text.substr (i, j - i);
            numberWords (digits.size() > 15 ? 0 : std::stoll (digits), t.spoken);
            if (j + 1 < text.size() && text[j] == '.' && std::isdigit ((unsigned char) text[j + 1]))
            {
                t.spoken.push_back ("point");
                ++j;
                while (j < text.size() && std::isdigit ((unsigned char) text[j]))
                {
                    t.spoken.push_back (kOnes[text[j] - '0']);
                    t.text += text[j];
                    ++j;
                }
            }
            tokens.push_back (t);
            i = j;
        }
        else
        {
            Token t;
            switch (c)
            {
                case '.': case '!': case '?': case '\n': t.pauseMs = 380; t.boundary = (char) (c == '\n' ? '.' : c); break;
                case ',': case ';': case ':': t.pauseMs = 180; t.boundary = ','; break;
                case '-': t.pauseMs = 90; t.boundary = ','; break;
                default: break;
            }
            if (t.pauseMs > 0)
            {
                // Merge runs like "?!" or "..." into one pause
                if (! tokens.empty() && tokens.back().pauseMs > 0)
                {
                    tokens.back().pauseMs = std::max (tokens.back().pauseMs, t.pauseMs);
                    if (t.boundary != ',') tokens.back().boundary = t.boundary;
                }
                else tokens.push_back (t);
            }
            ++i;
        }
    }

    // ---- Pronounce, and collect phrases for intonation ----
    struct PhraseInfo { int firstPhone; int lastPhone; char end; };
    std::vector<PhraseInfo> phrases;
    int phraseStart = 0;

    for (const auto& t : tokens)
    {
        if (t.pauseMs > 0)
        {
            if ((int) u->phones.size() > phraseStart)
                phrases.push_back ({ phraseStart, (int) u->phones.size() - 1, t.boundary });
            Phone p; p.id = PAUSE; p.dur = (float) t.pauseMs;
            u->phones.push_back (p);
            phraseStart = (int) u->phones.size();
            if (! u->phonemeText.empty()) u->phonemeText += (t.boundary == ',' ? " |" : " ||");
            continue;
        }

        for (const auto& spoken : t.spoken)
        {
            const Pron pron = pronounce (spoken, dict);
            if (pron.empty()) continue;

            Range wr;
            wr.first = (int) u->phones.size();
            const int wordIndex = (int) u->words.size();
            for (const auto& ps : pron)
            {
                Phone p;
                p.id = ps.id;
                p.stress = std::max (0, ps.stress);
                p.word = wordIndex;
                u->phones.push_back (p);
                if (! u->phonemeText.empty()) u->phonemeText += ' ';
                u->phonemeText += phoneme (ps.id).name;
                if (isVowelId (ps.id)) u->phonemeText += (char) ('0' + p.stress);
            }
            wr.last = (int) u->phones.size() - 1;
            u->words.push_back (wr);
            u->wordText.push_back (t.spoken.size() == 1 ? t.text : spoken);
            if (! u->phonemeText.empty()) u->phonemeText += ' ';
        }
    }
    if ((int) u->phones.size() > phraseStart)
        phrases.push_back ({ phraseStart, (int) u->phones.size() - 1, '.' });

    // ---- Syllables: each vowel is a nucleus; single consonants between vowels start the next syllable ----
    for (auto& w : u->words)
    {
        std::vector<int> nuclei;
        for (int k = w.first; k <= w.last; ++k) if (isVowelId (u->phones[(size_t) k].id)) nuclei.push_back (k);
        if (nuclei.empty()) { u->syllables.push_back ({ w.first, w.last, -1 }); continue; }

        int start = w.first;
        for (size_t n = 0; n < nuclei.size(); ++n)
        {
            int end = w.last;
            if (n + 1 < nuclei.size())
            {
                const int consonants = nuclei[n + 1] - nuclei[n] - 1;
                end = nuclei[n] + (consonants <= 1 ? 0 : 1);
            }
            u->syllables.push_back ({ start, end, nuclei[n] });
            start = end + 1;
        }
        w.sustain = nuclei.back();
    }

    // ---- Durations ----
    const int numPhones = (int) u->phones.size();
    std::vector<bool> phraseFinal ((size_t) numPhones, false);
    for (const auto& ph : phrases)
    {
        int lastVowel = -1;
        for (int k = ph.lastPhone; k >= ph.firstPhone; --k) if (isVowelId (u->phones[(size_t) k].id)) { lastVowel = k; break; }
        if (lastVowel >= 0) for (int k = lastVowel; k <= ph.lastPhone; ++k) phraseFinal[(size_t) k] = true;
    }

    for (int k = 0; k < numPhones; ++k)
    {
        auto& p = u->phones[(size_t) k];
        if (p.id == PAUSE) continue;
        const auto& d = phoneme (p.id);
        float dur = d.dur;
        if (isVowelId (p.id))
        {
            if (p.stress == 1) dur *= 1.15f;
            else if (p.stress == 2) dur *= 0.95f;
            else dur *= (p.id == phonemeId ("AH") || p.id == phonemeId ("IH")) ? 0.55f : 0.7f;
        }
        else
        {
            const bool prevCons = k > 0 && u->phones[(size_t) k - 1].id != PAUSE && ! isVowelId (u->phones[(size_t) k - 1].id);
            const bool nextCons = k + 1 < numPhones && u->phones[(size_t) k + 1].id != PAUSE && ! isVowelId (u->phones[(size_t) k + 1].id);
            if (prevCons || nextCons) dur *= 0.8f;
        }
        if (phraseFinal[(size_t) k]) dur *= isVowelId (p.id) ? 1.4f : 1.2f;
        p.dur = std::max (25.0f, dur * tune ("DUR", 0.78f));   // overall pace of normal conversation
    }

    // ---- Natural intonation: gentle declination, accents on stressed vowels, final rise or fall ----
    for (const auto& ph : phrases)
    {
        std::vector<int> vowels;
        for (int k = ph.firstPhone; k <= ph.lastPhone; ++k) if (isVowelId (u->phones[(size_t) k].id)) vowels.push_back (k);
        const int nv = (int) vowels.size();
        for (int n = 0; n < nv; ++n)
        {
            auto& p = u->phones[(size_t) vowels[(size_t) n]];
            const float decl = nv > 1 ? 0.10f - 0.20f * (float) n / (float) (nv - 1) : 0.0f;
            const float accent = p.stress == 1 ? 0.15f : (p.stress == 2 ? 0.06f : -0.03f);
            p.pitchStart = p.pitchEnd = decl + accent;
            if (n == nv - 1)
            {
                if (ph.end == '?')      p.pitchEnd = 0.38f;
                else if (ph.end == ',') p.pitchEnd = p.pitchStart + 0.06f;
                else if (ph.end == '!') { p.pitchStart += 0.12f; p.pitchEnd = -0.15f; }
                else                    p.pitchEnd = -0.18f;
            }
        }
        // Consonants carry the pitch of the vowel before them
        float carry = nv > 0 ? u->phones[(size_t) vowels[0]].pitchStart : 0.0f;
        for (int k = ph.firstPhone; k <= ph.lastPhone; ++k)
        {
            auto& p = u->phones[(size_t) k];
            if (isVowelId (p.id)) carry = p.pitchEnd;
            else p.pitchStart = p.pitchEnd = carry;
        }
    }

    return u;
}
} // namespace talk
