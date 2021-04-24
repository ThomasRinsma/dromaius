#include "games.h"

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


void gameGUI_pokemon_red(Dromaius *emu) {
	uint8_t const zero = 0;
	uint8_t const twofivefour = 254;
	uint8_t const twofivefive = 255;

	// https://datacrystal.romhacking.net/wiki/Pok%C3%A9mon_Red/Blue:RAM_map
	auto const playerName = getPokeStringAt(emu, 0xD158, 0x10);
	auto const rivalName = getPokeStringAt(emu, 0xD34A, 0x10);
	ImGui::Text("player: %s, rival: %s", playerName.c_str(), rivalName.c_str());

	// TODO grab Mon info
	auto monNick = getPokeStringAt(emu, 0xD2B5, 0x10);
	ImGui::Text("1st mon: %s", monNick.c_str());

	// Items in inventory
	// auto itemCount = emu->memory.readByte(0xD31D);
	// for (int i = 0; i < itemCount; ++i) {
	// 	auto itemNum = emu->memory.readByte(0xD31E + i * 2);
	// 	auto itemQnt = emu->memory.readByte(0xD31F + i * 2);
	// 	ImGui::Text("item %d x %d\n", itemNum, itemQnt);
	// }

	if (ImGui::CollapsingHeader("Items (player)")) {
		// Determine number of items


		ImGui::BeginTable("player items", 2, ImGuiTableFlags_Borders);
		int i;
		for (i = 0; i < 20; ++i) {
			// List is 0xFF-terminated
			if (emu->memory.workram[0x131E + i * 2] == 0xFF) {
				++i;
				break;
			}

			ImGui::TableNextColumn();
			ImGui::PushID(i*2);
			ImGui::PushItemWidth(-1);
			ImGui::DragScalar("##item", ImGuiDataType_U8, &emu->memory.workram[0x131E + i * 2], 0.5, &zero, &twofivefour, "Item: %d");
			ImGui::PopItemWidth();
			ImGui::PopID();

			ImGui::TableNextColumn();
			ImGui::PushID(i*2+1);
			ImGui::PushItemWidth(-1);
			ImGui::DragScalar("##count", ImGuiDataType_U8, &emu->memory.workram[0x131F + i * 2], 0.5, &zero, &twofivefive, "Count: %d");
			ImGui::PopItemWidth();
			ImGui::PopID();
		}
		ImGui::EndTable();

		// If the list is not full
		if (i < 20 && ImGui::Button("Add item")) {
			// Remove list terminator
			emu->memory.workram[0x131E + (i-1) * 2] = 0x00;

			// Add a new terminator if this is not the last item
			if (i < 20) {
				emu->memory.workram[0x131E + i * 2] = 0xFF;
			}

			// Update total item count (0xD31D)
			emu->memory.workram[0x131D] = i;
		}
		// ImGui::SameLine();
		if (i > 1 && ImGui::Button("Remove last")) {
			emu->memory.workram[0x131E + (i-2) * 2] = 0xFF;
		}


	}

}