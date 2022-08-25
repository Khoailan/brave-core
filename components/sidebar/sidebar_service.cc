/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/sidebar/sidebar_service.h"

#include <algorithm>
#include <codecvt>
#include <iterator>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "brave/components/l10n/common/locale_util.h"
#include "brave/components/sidebar/constants.h"
#include "brave/components/sidebar/pref_names.h"
#include "brave/components/sidebar/sidebar_item.h"
#include "components/grit/brave_components_strings.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using version_info::Channel;

namespace sidebar {

namespace {

// A list of preferred item types
constexpr SidebarItem::BuiltInItemType kPreferredPanelOrder[] = {
    SidebarItem::BuiltInItemType::kReadingList,
    SidebarItem::BuiltInItemType::kBookmarks};

SidebarItem GetBuiltInItemForType(SidebarItem::BuiltInItemType type) {
  switch (type) {
    case SidebarItem::BuiltInItemType::kBraveTalk:
      return SidebarItem::Create(GURL(kBraveTalkURL),
                                 brave_l10n::GetLocalizedResourceUTF16String(
                                     IDS_SIDEBAR_BRAVE_TALK_ITEM_TITLE),
                                 SidebarItem::Type::kTypeBuiltIn,
                                 SidebarItem::BuiltInItemType::kBraveTalk,
                                 false);
    case SidebarItem::BuiltInItemType::kWallet:
      return SidebarItem::Create(GURL("chrome://wallet/"),
                                 brave_l10n::GetLocalizedResourceUTF16String(
                                     IDS_SIDEBAR_WALLET_ITEM_TITLE),
                                 SidebarItem::Type::kTypeBuiltIn,
                                 SidebarItem::BuiltInItemType::kWallet, false);
    case SidebarItem::BuiltInItemType::kBookmarks:
      return SidebarItem::Create(brave_l10n::GetLocalizedResourceUTF16String(
                                     IDS_SIDEBAR_BOOKMARKS_ITEM_TITLE),
                                 SidebarItem::Type::kTypeBuiltIn,
                                 SidebarItem::BuiltInItemType::kBookmarks,
                                 true);
    case SidebarItem::BuiltInItemType::kReadingList:
      return SidebarItem::Create(
          // TODO(petemill): Have these items created under brave/browser
          // so that we can access common strings, like IDS_READ_LATER_TITLE.
          brave_l10n::GetLocalizedResourceUTF16String(
              IDS_SIDEBAR_READING_LIST_ITEM_TITLE),
          SidebarItem::Type::kTypeBuiltIn,
          SidebarItem::BuiltInItemType::kReadingList, true);
    case SidebarItem::BuiltInItemType::kHistory:
      return SidebarItem::Create(GURL("chrome://history/"),
                                 brave_l10n::GetLocalizedResourceUTF16String(
                                     IDS_SIDEBAR_HISTORY_ITEM_TITLE),
                                 SidebarItem::Type::kTypeBuiltIn,
                                 SidebarItem::BuiltInItemType::kHistory, true);
    default:
      NOTREACHED();
  }
  return SidebarItem();
}

SidebarItem::BuiltInItemType GetBuiltInItemTypeForLegacyURL(
    const std::string& url) {
  // A previous version of prefs used the URL even for built-in items, and not
  // the |SidebarItem::BuiltInItemType|. Therefore, this list should not
  // need to be updated.
  if (url == "https://together.brave.com/" || url == "https://talk.brave.com/")
    return SidebarItem::BuiltInItemType::kBraveTalk;

  if (url == "chrome://wallet/")
    return SidebarItem::BuiltInItemType::kWallet;

  if (url == "chrome://sidebar-bookmarks.top-chrome/" ||
      url == "chrome://bookmarks/")
    return SidebarItem::BuiltInItemType::kBookmarks;

  if (url == "chrome://history/")
    return SidebarItem::BuiltInItemType::kHistory;

  NOTREACHED() << url;
  return SidebarItem::BuiltInItemType::kNone;
}

std::vector<SidebarItem> GetDefaultSidebarItems() {
  std::vector<SidebarItem> items;
  for (const auto& item_type : SidebarService::kDefaultBuiltInItemTypes) {
    items.push_back(GetBuiltInItemForType(item_type));
  }
  return items;
}

}  // namespace

// static
void SidebarService::RegisterProfilePrefs(PrefRegistrySimple* registry,
                                          version_info::Channel channel) {
  registry->RegisterListPref(kSidebarItems);
  registry->RegisterListPref(kSidebarHiddenBuiltInItems);
  registry->RegisterIntegerPref(
      kSidebarShowOption,
      channel == Channel::STABLE
          ? static_cast<int>(ShowSidebarOption::kShowNever)
          : static_cast<int>(ShowSidebarOption::kShowAlways));
  registry->RegisterIntegerPref(kSidebarItemAddedFeedbackBubbleShowCount, 0);
}

SidebarService::SidebarService(PrefService* prefs) : prefs_(prefs) {
  DCHECK(prefs_);
  MigratePrefSidebarBuiltInItemsToHidden();

  LoadSidebarItems();

  MigrateSidebarShowOptions();

  pref_change_registrar_.Init(prefs_);
  pref_change_registrar_.Add(
      kSidebarShowOption,
      base::BindRepeating(&SidebarService::OnPreferenceChanged,
                          base::Unretained(this)));
}

SidebarService::~SidebarService() = default;

void SidebarService::MigrateSidebarShowOptions() {
  auto option =
      static_cast<ShowSidebarOption>(prefs_->GetInteger(kSidebarShowOption));
  // Show on click is deprecated. Treat it as show on mouse over.
  if (option == ShowSidebarOption::kShowOnClick) {
    option = ShowSidebarOption::kShowOnMouseOver;
    prefs_->SetInteger(kSidebarShowOption,
                       static_cast<int>(ShowSidebarOption::kShowOnMouseOver));
  }
}

void SidebarService::MigratePrefSidebarBuiltInItemsToHidden() {
  // kSidebarItems pref used to contain built-in items which should be shown.
  // This was changed to store those in a separate pref which contains
  // built-in items the user has chosen to hide. However kSidebarItems still
  // has entries for built-in items so they can be re-ordered.
  // It only stores built-in items that should be hidden so that new
  // items will appear, and we can remove old items.
  auto* built_in_items_to_hide_preference =
      prefs_->FindPreference(kSidebarHiddenBuiltInItems);
  if (!built_in_items_to_hide_preference->IsDefaultValue()) {
    VLOG(1) << "Not migrating built-in items, migration already complete.";
    return;
  }
  auto* preference = prefs_->FindPreference(kSidebarItems);
  if (preference->IsDefaultValue()) {
    VLOG(1) << "Not migrating built-in items, pref is still default.";
    return;
  }
  // Only include items that were known prior to this migration
  std::vector<SidebarItem> built_in_items_to_hide;
  built_in_items_to_hide.push_back(
      GetBuiltInItemForType(SidebarItem::BuiltInItemType::kBraveTalk));
  built_in_items_to_hide.push_back(
      GetBuiltInItemForType(SidebarItem::BuiltInItemType::kWallet));
  built_in_items_to_hide.push_back(
      GetBuiltInItemForType(SidebarItem::BuiltInItemType::kBookmarks));

  // We will also correct built-in items which did not specify their type
  // and instead relied on Url matching to find the built-in type.
  bool items_are_modified = false;
  constexpr char kSidebarItemShouldRemoveKey[] = "should_remove";

  const auto& items = preference->GetValue()->GetList();
  VLOG(2) << "MigratePrefSidebarBuiltInItemsToHidden: item count is "
          << items.size();

  // Find built-in items in items pref and keep them visible. Clone so that we
  // can potentially modify and re-save.
  auto new_items = items.Clone();
  for (auto& item_value : new_items) {
    DVLOG(2) << "Found an item: " << item_value.DebugString();
    // Verify item is valid
    auto* item = item_value.GetIfDict();
    if (!item || item->empty()) {
      DVLOG(1) << "Item in prefs was not a valid dict: "
               << item_value.DebugString();
      continue;
    }
    // Only care about built-in type
    SidebarItem::Type type;
    const auto type_value = item->FindInt(kSidebarItemTypeKey);
    if (!type_value) {
      VLOG(1) << "Item has no type item";
      continue;
    }
    type = static_cast<SidebarItem::Type>(*type_value);
    if (type != SidebarItem::Type::kTypeBuiltIn) {
      VLOG(2) << "Item is not built-in type";
      continue;
    }
    // Found a built-in item to keep
    auto item_id = item->FindInt(kSidebarItemBuiltInItemTypeKey);
    if (!item_id.has_value()) {
      // Attempt to get item_id from the url, which was a legacy method of
      // storing the built-in type.
      VLOG(1) << "MigratePrefSidebarBuiltInItemsToHidden: A built-in item "
                 "was found in the older pref format without a valid id. "
                 "Attempting to migrate...";
      DVLOG(1) << "Pref list item was: " << item->DebugString();
      const auto* url = item->FindString(kSidebarItemURLKey);
      if (url->empty()) {
        // This should be impossible (a built-in item without a url or type),
        // but could happen if someone manually edited the settings file.
        VLOG(1)
            << "...could not migrate item, url was empty! Marking for removal.";
        item->Set(kSidebarItemShouldRemoveKey, true);
        items_are_modified = true;
        continue;
      }
      const auto item_type = GetBuiltInItemTypeForLegacyURL(*url);
      if (item_type == SidebarItem::BuiltInItemType::kNone) {
        // This should be impossible (a built-in item without a url or type),
        // but could happen if someone manually edited the settings file.
        VLOG(1)
            << "...could not migrate item, url was empty! Marking for removal.";
        item->Set(kSidebarItemShouldRemoveKey, true);
        items_are_modified = true;
        continue;
      }
      const auto item_id_parsed = static_cast<int>(item_type);
      // Mark this item to be updated
      items_are_modified = true;
      item->Set(kSidebarItemBuiltInItemTypeKey, item_id_parsed);
      item->Remove(kSidebarItemURLKey);
      // Proceed with new value
      item_id = item_id_parsed;
    }
    // Remember not to hide this item
    auto iter = base::ranges::find_if(
        built_in_items_to_hide, [&item_id](const auto& default_item) {
          return default_item.built_in_item_type ==
                 static_cast<SidebarItem::BuiltInItemType>(*item_id);
        });
    // It might be an item which is no longer is offered
    if (iter != built_in_items_to_hide.end()) {
      built_in_items_to_hide.erase(iter);
    } else {
      VLOG(1) << "A built-in item was found in the older pref format which is "
                 "no longer part of the default built-in items, id: "
              << *item_id;
    }
  }

  // Build new pref, if any have been marked for hiding
  ListPrefUpdate builtin_items_update(prefs_, kSidebarHiddenBuiltInItems);
  if (built_in_items_to_hide.size()) {
    for (const auto& item : built_in_items_to_hide) {
      DCHECK(item.type == SidebarItem::Type::kTypeBuiltIn);
      const auto value = static_cast<int>(item.built_in_item_type);
      VLOG(2) << "Marked for hiding built-in item with ID: " << value;
      base::Value item_type(value);
      builtin_items_update->Append(std::move(item_type));
    }
  } else {
    // Always store something so that we know migration is done
    // when pref isn't default value.
    builtin_items_update->ClearList();
  }

  // Fix items pref, if needed
  if (items_are_modified) {
    ListPrefUpdate items_update(prefs_, kSidebarItems);
    items_update->ClearList();
    for (const auto& item_value : new_items) {
      auto* item = item_value.GetIfDict();
      DCHECK(item);
      const auto should_ignore = item->FindBool(kSidebarItemShouldRemoveKey);
      if (should_ignore.value_or(false)) {
        continue;
      }
      items_update->Append(base::Value(item->Clone()));
    }
  }
}

void SidebarService::AddItem(const SidebarItem& item) {
  items_.push_back(item);

  for (Observer& obs : observers_) {
    // Index starts at zero.
    obs.OnItemAdded(item, items_.size() - 1);
  }

  UpdateSidebarItemsToPrefStore();
}

void SidebarService::RemoveItemAt(int index) {
  const SidebarItem removed_item = items_[index];

  for (Observer& obs : observers_)
    obs.OnWillRemoveItem(removed_item, index);

  items_.erase(items_.begin() + index);
  for (Observer& obs : observers_)
    obs.OnItemRemoved(removed_item, index);

  UpdateSidebarItemsToPrefStore();
}

void SidebarService::MoveItem(int from, int to) {
  DCHECK(items_.size() > static_cast<size_t>(from) &&
         items_.size() > static_cast<size_t>(to) && from >= 0 && to >= 0);

  if (from == to)
    return;

  const SidebarItem item = items_[from];
  items_.erase(items_.begin() + from);
  items_.insert(items_.begin() + to, item);

  for (Observer& obs : observers_)
    obs.OnItemMoved(item, from, to);

  UpdateSidebarItemsToPrefStore();
}

void SidebarService::UpdateSidebarItemsToPrefStore() {
  // Store all items in a list pref.
  // Each item gets an entry. Built in items only need their type, and are
  // only stored so we preserve their order. Custom items
  // need all their detail.
  // We also need to explicitly store which built-in items have been hidden
  // so that we know which new items the user has been exposed to and which
  // they've chosen to hide.
  ListPrefUpdate update(prefs_, kSidebarItems);
  update->ClearList();
  DVLOG(2) << "Serializing items (count: " << items_.size() << ")";

  // Serialize each item
  for (const auto& item : items_) {
    DVLOG(2) << "Adding item to pref list: "
             << static_cast<int>(item.built_in_item_type);
    base::Value dict(base::Value::Type::DICTIONARY);
    dict.SetIntKey(kSidebarItemTypeKey, static_cast<int>(item.type));
    dict.SetIntKey(kSidebarItemBuiltInItemTypeKey,
                   static_cast<int>(item.built_in_item_type));
    if (item.type != SidebarItem::Type::kTypeBuiltIn) {
      dict.SetStringKey(kSidebarItemURLKey, item.url.spec());
      dict.SetStringKey(kSidebarItemTitleKey, base::UTF16ToUTF8(item.title));
      dict.SetBoolKey(kSidebarItemOpenInPanelKey, item.open_in_panel);
    }
    update->Append(std::move(dict));
  }

  // Store which built-in items should be hidden
  ListPrefUpdate hide_builtin_update(prefs_, kSidebarHiddenBuiltInItems);
  hide_builtin_update->ClearList();
  // TODO(petemill): If we make any hidden-by-default built-in items,
  // then this logic needs to change to only consider shown-by-default items,
  // and perhaps use a dict for each item to store whether built-in item is
  // chosen to be added or removed.
  auto hidden_items = GetHiddenDefaultSidebarItems();
  for (const auto& hidden_item : hidden_items) {
    base::Value item_type(static_cast<int>(hidden_item.built_in_item_type));
    hide_builtin_update->Append(std::move(item_type));
  }
}

void SidebarService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SidebarService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::vector<SidebarItem> SidebarService::GetHiddenDefaultSidebarItems() const {
  const auto added_default_items = GetCurrentlyPresentBuiltInTypes();
  auto default_items = GetDefaultSidebarItems();

  default_items.erase(
      base::ranges::remove_if(default_items,
                              [&added_default_items](auto& item) {
                                return base::Contains(added_default_items,
                                                      item.built_in_item_type);
                              }),
      default_items.end());
  return default_items;
}

std::vector<SidebarItem::BuiltInItemType>
SidebarService::GetCurrentlyPresentBuiltInTypes() const {
  std::vector<SidebarItem::BuiltInItemType> items;
  base::ranges::for_each(items_, [&items](const auto& item) {
    if (IsBuiltInType(item))
      items.push_back(item.built_in_item_type);
  });
  return items;
}

SidebarService::ShowSidebarOption SidebarService::GetSidebarShowOption() const {
  return static_cast<ShowSidebarOption>(prefs_->GetInteger(kSidebarShowOption));
}

absl::optional<SidebarItem> SidebarService::GetDefaultPanelItem() const {
  absl::optional<SidebarItem> default_item;
  for (const auto& type : kPreferredPanelOrder) {
    auto found_item_iter = base::ranges::find_if(
        items_,
        [type](SidebarItem item) { return (item.built_in_item_type == type); });
    if (found_item_iter != items_.end()) {
      default_item = *found_item_iter;
      DCHECK_EQ(default_item->open_in_panel, true);
      break;
    }
  }
  return default_item;
}

void SidebarService::SetSidebarShowOption(ShowSidebarOption show_options) {
  DCHECK_NE(ShowSidebarOption::kShowOnClick, show_options);
  prefs_->SetInteger(kSidebarShowOption, static_cast<int>(show_options));
}

void SidebarService::LoadSidebarItems() {
  auto default_items_to_add = GetDefaultSidebarItems();

  // Pref for custom items and custom order
  auto* preference = prefs_->FindPreference(kSidebarItems);
  if (!preference->IsDefaultValue()) {
    const auto& items = preference->GetValue()->GetList();
    for (const auto& item : items) {
      DVLOG(2) << "load: " << item.DebugString();
      SidebarItem::Type type;
      if (const auto type_value = item.FindIntKey(kSidebarItemTypeKey)) {
        type = static_cast<SidebarItem::Type>(*type_value);
      } else {
        continue;
      }
      // Always use latest properties for built-in type item.
      if (type == SidebarItem::Type::kTypeBuiltIn) {
        const auto built_in_type_value =
            item.FindIntKey(kSidebarItemBuiltInItemTypeKey);
        if (!built_in_type_value.has_value()) {
          VLOG(1) << "built-in item did not have a type: "
                  << item.DebugString();
          continue;
        }
        auto id =
            static_cast<SidebarItem::BuiltInItemType>(*built_in_type_value);
        auto iter = base::ranges::find_if(
            default_items_to_add, [id](const auto& default_item) {
              return default_item.built_in_item_type == id;
            });
        // It might be an item which is no longer is offered as built-in
        if (iter == default_items_to_add.end()) {
          VLOG(1) << "item not found: " << item.DebugString();
          continue;
        }
        // Valid built-in item, add it
        items_.emplace_back(*std::make_move_iterator(iter));
        default_items_to_add.erase(iter);
        continue;
      }
      // Deserialize custom item
      std::string url;
      if (const auto* value = item.FindStringKey(kSidebarItemURLKey)) {
        url = *value;
      } else {
        continue;
      }
      // Open in panel for custom items is not yet supported
      bool open_in_panel = false;
      std::string title;
      if (const auto* value = item.FindStringKey(kSidebarItemTitleKey)) {
        title = *value;
      }
      items_.push_back(SidebarItem::Create(
          GURL(url), base::UTF8ToUTF16(title), type,
          SidebarItem::BuiltInItemType::kNone, open_in_panel));
    }
  }

  //
  // Add built-in items which haven't been shown or hidden.
  //
  // Don't consider built-in items that the user has already hidden.
  auto* hidden_built_in_preference =
      prefs_->FindPreference(kSidebarHiddenBuiltInItems);
  if (!hidden_built_in_preference->IsDefaultValue()) {
    for (const auto& item : hidden_built_in_preference->GetValue()->GetList()) {
      // Don't show this built-in item
      const auto id = static_cast<SidebarItem::BuiltInItemType>(item.GetInt());
      DVLOG(2) << "hide built-in item with id: " << item.GetInt();
      auto iter =
          std::find_if(default_items_to_add.begin(), default_items_to_add.end(),
                       [id](const auto& default_item) {
                         return default_item.built_in_item_type ==
                                static_cast<SidebarItem::BuiltInItemType>(id);
                       });
      if (iter != default_items_to_add.end()) {
        default_items_to_add.erase(iter);
      } else {
        DLOG(ERROR) << "Asked to hide an item that was already asked to show. "
                       "This indicates something is wrong with the "
                       "serialization process. Id was: "
                    << item.GetInt();
      }
    }
  }

  // Add the items the user has never seen (or never persisted).
  // Get the initial order of items so that we can attempt to
  // insert at the intended order.
  for (const auto& item : default_items_to_add) {
    const auto* default_item_iter = base::ranges::find(
        SidebarService::kDefaultBuiltInItemTypes, item.built_in_item_type);
    auto default_index = default_item_iter -
                         std::begin(SidebarService::kDefaultBuiltInItemTypes);
    // Add at the default index for the first time. For users which haven't
    // changed any order, or removed items, this will be at the intentional
    // index. For users who have re-ordered, this will be different but still
    // acceptable. It will be a minority of cases where it gets inserted in to
    // the middle of custom items, but that will still work.
    auto index = std::min(static_cast<int>(default_index),
                          static_cast<int>(items_.size()));
    VLOG(2) << "Inserting built-in item ("
            << static_cast<int>(item.built_in_item_type)
            << " with default index: " << default_index
            << " at actual index: " << index;
    items_.insert(items_.begin() + index, std::move(item));
  }
}

void SidebarService::OnPreferenceChanged(const std::string& pref_name) {
  if (pref_name == kSidebarShowOption) {
    for (Observer& obs : observers_)
      obs.OnShowSidebarOptionChanged(GetSidebarShowOption());
    return;
  }
}

}  // namespace sidebar
