#include "games.h"

// https://datacrystal.romhacking.net/wiki/Pok%C3%A9mon_Red/Blue:RAM_map



std::map<uint8_t, char const *> poke_item_names = {
	{0x01, "Master Ball"},
	{0x02, "Ultra Ball"},
	{0x03, "Great Ball"},
	{0x04, "Poké Ball"},
	{0x05, "Town Map"},
	{0x06, "Bicycle"},
	{0x07, "?????"},
	{0x08, "Safari Ball"},
	{0x09, "Pokédex"},
	{0x0A, "Moon Stone"},
	{0x0B, "Antidote"},
	{0x0C, "Burn Heal"},
	{0x0D, "Ice Heal"},
	{0x0E, "Awakening"},
	{0x0F, "Parlyz Heal"},
	{0x10, "Full Restore"},
	{0x11, "Max Potion"},
	{0x12, "Hyper Potion"},
	{0x13, "Super Potion"},
	{0x14, "Potion"},
	{0x15, "BoulderBadge"},
	{0x16, "CascadeBadge"},
	{0x17, "ThunderBadge"},
	{0x18, "RainbowBadge"},
	{0x19, "SoulBadge"},
	{0x1A, "MarshBadge"},
	{0x1B, "VolcanoBadge"},
	{0x1C, "EarthBadge"},
	{0x1D, "Escape Rope"},
	{0x1E, "Repel"},
	{0x1F, "Old Amber"},
	{0x20, "Fire Stone"},
	{0x21, "Thunderstone"},
	{0x22, "Water Stone"},
	{0x23, "HP Up"},
	{0x24, "Protein"},
	{0x25, "Iron"},
	{0x26, "Carbos"},
	{0x27, "Calcium"},
	{0x28, "Rare Candy"},
	{0x29, "Dome Fossil"},
	{0x2A, "Helix Fossil"},
	{0x2B, "Secret Key"},
	{0x2C, "?????"},
	{0x2D, "Bike Voucher"},
	{0x2E, "X Accuracy"},
	{0x2F, "Leaf Stone"},
	{0x30, "Card Key"},
	{0x31, "Nugget"},
	{0x32, "PP Up*"},
	{0x33, "Poké Doll"},
	{0x34, "Full Heal"},
	{0x35, "Revive"},
	{0x36, "Max Revive"},
	{0x37, "Guard Spec."},
	{0x38, "Super Repel"},
	{0x39, "Max Repel"},
	{0x3A, "Dire Hit"},
	{0x3B, "Coin"},
	{0x3C, "Fresh Water"},
	{0x3D, "Soda Pop"},
	{0x3E, "Lemonade"},
	{0x3F, "S.S. Ticket"},
	{0x40, "Gold Teeth"},
	{0x41, "X Attack"},
	{0x42, "X Defend"},
	{0x43, "X Speed"},
	{0x44, "X Special"},
	{0x45, "Coin Case"},
	{0x46, "Oak's Parcel"},
	{0x47, "Itemfinder"},
	{0x48, "Silph Scope"},
	{0x49, "Poké Flute"},
	{0x4A, "Lift Key"},
	{0x4B, "Exp. All"},
	{0x4C, "Old Rod"},
	{0x4D, "Good Rod"},
	{0x4E, "Super Rod"},
	{0x4F, "PP Up"},
	{0x50, "Ether"},
	{0x51, "Max Ether"},
	{0x52, "Elixer"},
	{0x53, "Max Elixer"},
	{0xC4, "HM01"},
	{0xC5, "HM02"},
	{0xC6, "HM03"},
	{0xC7, "HM04"},
	{0xC8, "HM05"},
	{0xC9, "TM01"},
	{0xCA, "TM02"},
	{0xCB, "TM03"},
	{0xCC, "TM04"},
	{0xCD, "TM05"},
	{0xCE, "TM06"},
	{0xCF, "TM07"},
	{0xD0, "TM08"},
	{0xD1, "TM09"},
	{0xD2, "TM10"},
	{0xD3, "TM11"},
	{0xD4, "TM12"},
	{0xD5, "TM13"},
	{0xD6, "TM14"},
	{0xD7, "TM15"},
	{0xD8, "TM16"},
	{0xD9, "TM17"},
	{0xDA, "TM18"},
	{0xDB, "TM19"},
	{0xDC, "TM20"},
	{0xDD, "TM21"},
	{0xDE, "TM22"},
	{0xDF, "TM23"},
	{0xE0, "TM24"},
	{0xE1, "TM25"},
	{0xE2, "TM26"},
	{0xE3, "TM27"},
	{0xE4, "TM28"},
	{0xE5, "TM29"},
	{0xE6, "TM30"},
	{0xE7, "TM31"},
	{0xE8, "TM32"},
	{0xE9, "TM33"},
	{0xEA, "TM34"},
	{0xEB, "TM35"},
	{0xEC, "TM36"},
	{0xED, "TM37"},
	{0xEE, "TM38"},
	{0xEF, "TM39"},
	{0xF0, "TM40"},
	{0xF1, "TM41"},
	{0xF2, "TM42"},
	{0xF3, "TM43"},
	{0xF4, "TM44"},
	{0xF5, "TM45"},
	{0xF6, "TM46"},
	{0xF7, "TM47"},
	{0xF8, "TM48"},
	{0xF9, "TM49"},
	{0xFA, "TM50"},
	{0xFB, "TM51"},
	{0xFC, "TM52"},
	{0xFD, "TM53"},
	{0xFE, "TM54"},
	{0xFF, "TM55"}
};

// Pokemon data struct as it appears in memory
typedef struct pokemon_s
{
	
	// D16B - Pokémon (Again)
	uint8_t index;
	// D16C-D16D - Current HP
	uint16_t hp;
	// D16E - 'Level' (not the actual level, see the notes article)
	uint8_t pseudoLevel;
	// D16F - Status (Poisoned, Paralyzed, etc.)
	uint8_t status; 
	// D170 - Type 1
	uint8_t types[2];
	// D172 - Catch rate/Held item (When traded to Generation II)
	uint8_t catchRate;
	// D173 - Move 1
	uint8_t moves[4];
	// D177-D178 - Trainer ID
	uint16_t trainerId;
	// D179-D17B - Experience
	uint8_t exp[3];
	// D17C-D17D - HP EV
	uint16_t evHp;
	// D17E-D17F - Attack EV
	uint16_t evAttack;
	// D180-D181 - Defense EV
	uint16_t evDefense;
	// D182-D183 - Speed EV
	uint16_t evSpeed;
	// D184-D185 - Special EV
	uint16_t evSpecial;
	// D186 - Attack/Defense IV
	uint8_t ivAttackDefense;
	// D187 - Speed/Special IV
	uint8_t ivSpeedSpecial;
	// D188 - PP Move 1
	uint8_t pp[4];
	// D18C - Level (actual level)
	uint8_t level;
	// D18D-D18E - Max HP
	uint16_t maxHp;
	// D18F-D190 - Attack
	uint16_t attack;
	// D191-D192 - Defense
	uint16_t defense;
	// D193-D194 - Speed
	uint16_t speed;
	// D195-D196 - Special
	uint16_t special;

} __attribute__((packed)) pokemon_t;

// Read at most `length` poke-encoded characters from `addr`
std::string getPokeStringAt(Dromaius *emu, uint16_t addr, uint16_t len) {
	std::string str;

	const char charMap[255] = 
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ():;[]abcdefghijklmnopqrstuvwxyzedlstv"
		"                                "
		"\"PM-rm?!.   >>vM$*./,F0123456789";

	for (int i = 0; i < len; ++i) {
		uint8_t b = emu->memory.readByte(addr + i);
		if (b == 0x50) {
			// 0x50 is the string delimiter
			break;
		}

		// Meaningful chars start at 0x80
		if (b < 0x80) {
			str.push_back(' ');
		} else {
			str.push_back(charMap[b - 0x80]);
		}
	}

	return str;
}

void renderBadges(Dromaius *emu) {
	// Bitmap at 0xD356
	ImGui::CheckboxFlags("Boulder", (int *)&emu->memory.workram[0x1356], 0x01);
	ImGui::SameLine();
	ImGui::CheckboxFlags("Cascade", (int *)&emu->memory.workram[0x1356], 0x02);
	ImGui::CheckboxFlags("Thunder", (int *)&emu->memory.workram[0x1356], 0x04);
	ImGui::SameLine();
	ImGui::CheckboxFlags("Rainbow", (int *)&emu->memory.workram[0x1356], 0x08);
	ImGui::CheckboxFlags("Soul   ", (int *)&emu->memory.workram[0x1356], 0x10);
	ImGui::SameLine();
	ImGui::CheckboxFlags("Marsh  ", (int *)&emu->memory.workram[0x1356], 0x20);
	ImGui::CheckboxFlags("Volcano", (int *)&emu->memory.workram[0x1356], 0x40);
	ImGui::SameLine();
	ImGui::CheckboxFlags("Earth  ", (int *)&emu->memory.workram[0x1356], 0x80);
}

void renderItems(Dromaius *emu, bool stored = false) {
	uint8_t const zero = 0;
	uint8_t const twofivefour = 254;
	uint8_t const twofivefive = 255;

	uint16_t baseAddr;
	uint8_t max;

	if (stored) {
		max = 50;
		baseAddr = 0xD53A - 0xC000;
	}
	else {
		max = 20;
		baseAddr = 0xD31D - 0xC000;
	}
	
	ImGui::PushID(stored);
	ImGui::BeginTable("player items", 2, ImGuiTableFlags_Borders);
	ImGui::TableSetupColumn("Item");
	ImGui::TableSetupColumn("Count");
	ImGui::TableHeadersRow();

	int i;
	for (i = 0; i < max; ++i) {
		// List is 0xFF-terminated
		if (emu->memory.workram[baseAddr + 1 + i * 2] == 0xFF) {
			++i;
			break;
		}

		ImGui::TableNextColumn();
		ImGui::PushID(i*2);
		ImGui::PushItemWidth(-1);
		uint8_t itemNr = emu->memory.workram[baseAddr + 1 + i * 2];
		if (poke_item_names.contains(itemNr)) {
			char displayBuf[100];
			sprintf(displayBuf, "%d (%s)", itemNr,
				poke_item_names[itemNr]);
			ImGui::DragScalar("##item", ImGuiDataType_U8, &emu->memory.workram[baseAddr + 1 + i * 2], 0.5, &zero, &twofivefour, displayBuf);
		} else {
			ImGui::DragScalar("##item", ImGuiDataType_U8, &emu->memory.workram[baseAddr + 1 + i * 2], 0.5, &zero, &twofivefour);
		}
		ImGui::PopItemWidth();
		ImGui::PopID();

		ImGui::TableNextColumn();
		ImGui::PushID(i*2+1);
		ImGui::PushItemWidth(-1);
		ImGui::DragScalar("##count", ImGuiDataType_U8, &emu->memory.workram[baseAddr + 2 + i * 2], 0.5, &zero, &twofivefive, "Count: %d");
		ImGui::PopItemWidth();
		ImGui::PopID();
	}
	ImGui::EndTable();

	// If the list is not full
	if (i < max && ImGui::Button("Add item")) {
		// Remove list terminator
		emu->memory.workram[baseAddr + 1 + (i-1) * 2] = 0x00;

		// Add a new terminator if this is not the last item
		if (i < max) {
			emu->memory.workram[baseAddr + 1 + i * 2] = 0xFF;
		}

		// Update total item count (0xD31D)
		emu->memory.workram[baseAddr] = i;
	}
	// ImGui::SameLine();
	if (i > 1 && ImGui::Button("Remove last")) {
		emu->memory.workram[baseAddr + 1 + (i-2) * 2] = 0xFF;
	}
	ImGui::PopID();
}

void renderParty(Dromaius *emu) {

	uint16_t partyBase = 0xD163 - 0xC000;
	uint16_t trainerNameBase = 0xD273;
	uint16_t nicknameBase = 0xD2B5;
	uint16_t monDataBase = 0xD16B - 0xC000;

	uint8_t const s = 1;

	uint8_t partyCnt = emu->memory.workram[partyBase];
	for (int i = 0; i < partyCnt; ++i) {
		uint8_t mon = emu->memory.workram[partyBase + 1 + i];
		auto monNick = getPokeStringAt(emu, nicknameBase + i * 10, 10);
		auto monTrainer = getPokeStringAt(emu, trainerNameBase + i * 10, 10);
		auto monData = (pokemon_t *)&emu->memory.workram[monDataBase + sizeof(pokemon_t) * i];


		ImGui::PushID(i);
		auto const title = monNick + ", lvl " + std::to_string(monData->level) + "###" + std::to_string(i);
		if (ImGui::TreeNode(title.c_str())) {

			ImGui::Text("Trainer: %s, ID: %d", monTrainer.c_str(), monData->trainerId);
			ImGui::InputScalar("Index nr.", ImGuiDataType_U8, &emu->memory.workram[partyBase + 1 + i], &s);
			// Make sure both datapoints are in sync
			monData->index = emu->memory.workram[partyBase + 1 + i];

			ImGui::InputScalar("Level", ImGuiDataType_U8, &monData->level, &s);
			ImGui::InputScalar("HP", ImGuiDataType_U8, &monData->hp, &s);
			ImGui::InputScalar("Max HP", ImGuiDataType_U8, &monData->maxHp, &s);
			ImGui::InputScalar("Status", ImGuiDataType_U8, &monData->status, &s);
			ImGui::InputScalar("Type 1", ImGuiDataType_U8, &monData->types[0], &s);
			ImGui::InputScalar("Type 2", ImGuiDataType_U8, &monData->types[1], &s);

			if (ImGui::TreeNode("Moveset")) {
				ImGui::BeginTable("moves", 2, ImGuiTableFlags_BordersInnerV);
				ImGui::TableNextColumn();
				ImGui::InputScalar("Move 1", ImGuiDataType_U8, &monData->moves[0], &s);
				ImGui::TableNextColumn();
				ImGui::InputScalar("PP##1", ImGuiDataType_U8, &monData->pp[0], &s);
				ImGui::TableNextColumn();
				ImGui::InputScalar("Move 2", ImGuiDataType_U8, &monData->moves[1], &s);
				ImGui::TableNextColumn();
				ImGui::InputScalar("PP##2", ImGuiDataType_U8, &monData->pp[1], &s);
				ImGui::TableNextColumn();
				ImGui::InputScalar("Move 3", ImGuiDataType_U8, &monData->moves[2], &s);
				ImGui::TableNextColumn();
				ImGui::InputScalar("PP##3", ImGuiDataType_U8, &monData->pp[2], &s);
				ImGui::TableNextColumn();
				ImGui::InputScalar("Move 4", ImGuiDataType_U8, &monData->moves[3], &s);
				ImGui::TableNextColumn();
				ImGui::InputScalar("PP##4", ImGuiDataType_U8, &monData->pp[3], &s);
				ImGui::EndTable();
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Stats")) {
				ImGui::InputScalar("Attack", ImGuiDataType_U8, &monData->attack, &s);
				ImGui::InputScalar("Defense", ImGuiDataType_U8, &monData->defense, &s);
				ImGui::InputScalar("Speed", ImGuiDataType_U8, &monData->speed, &s);
				ImGui::InputScalar("Special", ImGuiDataType_U8, &monData->special, &s);
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("EVs")) {
				ImGui::InputScalar("HP", ImGuiDataType_U8, &monData->evHp, &s);
				ImGui::InputScalar("Attack", ImGuiDataType_U8, &monData->evAttack, &s);
				ImGui::InputScalar("Defense", ImGuiDataType_U8, &monData->evDefense, &s);
				ImGui::InputScalar("Speed", ImGuiDataType_U8, &monData->evSpeed, &s);
				ImGui::InputScalar("Special", ImGuiDataType_U8, &monData->evSpecial, &s);
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("IVs")) {
				ImGui::InputScalar("Attack/Defense", ImGuiDataType_U8, &monData->ivAttackDefense, &s);
				ImGui::InputScalar("Speed/Special", ImGuiDataType_U8, &monData->ivSpeedSpecial, &s);
				ImGui::TreePop();
			}

			ImGui::TreePop();
		}

		ImGui::PopID();
	}


}

void gameGUI_pokemon_red(Dromaius *emu) {

	
	auto const playerName = getPokeStringAt(emu, 0xD158, 0x10);
	auto const rivalName = getPokeStringAt(emu, 0xD34A, 0x10);

	// Basic info
	ImGui::Text("player: %s, rival: %s", playerName.c_str(), rivalName.c_str());

	if (ImGui::CollapsingHeader("Badges")) {
		renderBadges(emu);
	}

	if (ImGui::CollapsingHeader("Items (player)")) {
		renderItems(emu);
	}

	if (ImGui::CollapsingHeader("Items (storage)")) {
		renderItems(emu, true);
	}

	if (ImGui::CollapsingHeader("Pokemon party")) {
		renderParty(emu);
	}

}