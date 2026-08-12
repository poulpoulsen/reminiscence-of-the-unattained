#pragma once
#include "plugin.hpp"

// ---------------------------------------------------------------------
// Panel-Beschriftungs-System (Session "Panel Labeling", Variante B: reine
// Code/NanoVG-Labels statt SVG-Artwork).
//
// WICHTIG (Session "Fehlende Labels auf Trigger Expander"): Diese Struct
// lag vorher als DREI separate, lokale Kopien in Marf248.cpp,
// Marf248TriggerExpander.cpp und Marf248CvExpander.cpp - mit der
// Begruendung "trivialer, headerloser Widget, kein ODR-Risiko". Das war
// falsch: die Kopie in Marf248.cpp war seit der Preset-Beschriftungs-
// Session (Rotation fuer vertikale Preset-Labels) um das Member
// rotationDeg UND einen 6. Konstruktor-Parameter erweitert worden, waehrend
// Marf248TriggerExpander.cpp und Marf248CvExpander.cpp weiterhin die aeltere
// 5-Parameter-Version enthielten. Drei Definitionen desselben Namens
// struct PanelLabel in derselben .so, die sich in Groesse/Layout
// unterscheiden, sind ein klassischer One-Definition-Rule-Verstoss
// (undefiniertes Verhalten) - der Linker waehlt (unspezifiziert) EINE
// Definition fuer alle Aufrufer. Landete dabei die 6-Parameter-Version im
// Trigger-Expander-Code, blieb rotationDeg dort uninitialisiert
// (Konstruktor schreibt nur 5 der 6 Member) - je nach zufaelligem
// Speicherinhalt drehte das Label unsichtbar aus dem Panel heraus (keine
// sichtbare Beschriftung, kein Crash) oder das abweichende Speicherlayout
// korrumpierte den Heap und liess Rack spaeter beim Zerstoeren des Widgets
// abstuerzen (Backtrace zeigte PanelLabel::~PanelLabel() /
// Marf248TriggerExpanderWidget::~Marf248TriggerExpanderWidget()).
//
// Ab jetzt genau EINE Definition hier in einem gemeinsamen Header, von
// allen drei Modulen per #include eingebunden - damit ist ODR-Konformitaet
// strukturell garantiert statt nur "hoffentlich synchron gehalten".
//
// SCHRIFTGROESSE (Korrektur nach erstem Test, war zu klein): NVG-
// Fontgroessen sind in Rack-px angegeben, 1mm entspricht ca. 2.9528px
// (SVG_DPI 75 / MM_PER_IN 25.4). fontSize=6 (erster Entwurf) ergab nur
// ca. 1.4mm Kappenhoehe - kaum lesbar. fontSize=9 (Standard-Default seit
// dieser Session, wie zuvor) ergibt ca. 2.1mm Kappenhoehe / ca. 3.7mm
// Zeilenhoehe - spuerbar besser lesbar, ohne das Layout komplett zu
// sprengen.
//
// Nunito-Bold.ttf liegt unter res/fonts/Nunito-Bold.ttf im Plugin-
// Verzeichnis. Ohne diese Datei liefert loadFont() einen leeren Font
// (font->handle == -1) und draw() ueberspringt den Text still (kein
// Crash, aber keine sichtbare Beschriftung) - siehe Guard in
// PanelLabel::draw().

// Grad -> Radiant, fuer PanelLabel::draw() (siehe rotationDeg dort). Eigene
// Konstante statt M_PI: die ist keine C++-Standardkonstante, sondern nur
// bedingt ueber <math.h>/<cmath> verfuegbar (Compiler-/Flag-abhaengig) -
// das hatte bereits einen Rack-Absturz verursacht. Eine selbst definierte
// Konstante ist genauso sicher (keine Header-Abhaengigkeit), aber lesbarer
// als eine unkommentierte Zahl im Funktionsaufruf.
static constexpr float DEG_TO_RAD = 0.017453293f; // pi/180

struct PanelLabel : Widget {
	std::string text;
	NVGcolor color;
	float fontSize;
	int align;
	// 0 = normale horizontale Beschriftung. Ungleich 0 (Grad, im
	// Uhrzeigersinn) fuer vertikal laufenden Text (siehe addLabelVertical()
	// in Marf248.cpp, Preset-Ziffern) - rotiert um den Anker-Punkt
	// (box.pos), bevor bei (0,0) gezeichnet wird, daher bleibt die
	// Textmitte exakt auf dem Anker, unabhaengig vom Rotationswinkel.
	float rotationDeg;
	std::shared_ptr<window::Font> font;

	PanelLabel(Vec posMM, std::string text, NVGcolor color, float fontSize = 9.f,
	           int align = NVG_ALIGN_CENTER | NVG_ALIGN_TOP, float rotationDeg = 0.f)
		: text(std::move(text)), color(color), fontSize(fontSize), align(align), rotationDeg(rotationDeg) {
		box.pos = mm2px(posMM);
		box.size = Vec(0.f, 0.f); // reines Text-Overlay, keine eigene Hitbox
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Nunito-Bold.ttf"));
	}

	void draw(const DrawArgs& args) override {
		if (!font || font->handle < 0)
			return; // Font fehlt (siehe Kommentar oben) - still ueberspringen statt crashen
		nvgSave(args.vg);
		if (rotationDeg != 0.f)
			nvgRotate(args.vg, rotationDeg * DEG_TO_RAD);
		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, fontSize);
		nvgTextAlign(args.vg, align);
		nvgTextLetterSpacing(args.vg, 0.f);
		nvgFillColor(args.vg, color);
		nvgText(args.vg, 0.f, 0.f, text.c_str(), NULL);
		nvgRestore(args.vg);
	}
};

// Farbkategorien (Kinovice-Entscheidung: "nach Sektion/Funktion
// eingefaerbt"). Ueber alle drei Module hinweg konsistent, daher ebenfalls
// hier zentral statt pro Modul dupliziert:
// - System: modulweite Housekeeping-Trigger (Clear/Reset/Preset)
// - Section A / Section B: alles was eine Section-Kennung traegt
// - StageMod: Elemente, die modulweit pro Stage wirken (Skip...Mute,
//   Time/Curve-CV auf dem CV-Expander) - unabhaengig von Section A/B.
static const NVGcolor COLOR_SYSTEM    = nvgRGB(70, 70, 68);
static const NVGcolor COLOR_SECTION_A = nvgRGB(30, 100, 200);
static const NVGcolor COLOR_SECTION_B = nvgRGB(210, 110, 20);
static const NVGcolor COLOR_STAGEMOD  = nvgRGB(0, 140, 130);
// COLOR_MUSICAL wird nur vom Trigger Expander (Scale/Key) benutzt - dort
// lokal belassen statt hier, da nicht modulweit genutzt.
