#pragma once

namespace Utils {

	inline std::map<std::string_view,bool> menu_blocks = {
         {RE::InventoryMenu::MENU_NAME, true},
        {RE::ContainerMenu::MENU_NAME, true},
        {RE::BarterMenu::MENU_NAME, true},
    };

	template <typename MenuType>
    RE::StandardItemData* GetSelectedItemData() {
        if (menu_blocks.contains(MenuType::MENU_NAME) && menu_blocks.at(MenuType::MENU_NAME)) {
            return nullptr;
		}
        if (const auto ui = RE::UI::GetSingleton()) {
            if (const auto menu = ui->GetMenu<MenuType>()) {
	            if (RE::ItemList* a_itemList = menu->GetRuntimeData().itemList) {
		            if (auto* item = a_itemList->GetSelectedItem()) {
			            return &item->data;
		            }
	            }
            }
        }
		return nullptr;
    }

	RE::StandardItemData* GetSelectedItemDataInMenu(std::string& a_menuOut);

    const char* GetModelPath(RE::TESForm* a_form, RE::Actor* a_actor=nullptr);
    void GetModel(RE::TESForm* a_form, RE::NiPointer<RE::NiAVObject>& a_out);
}