#include "data.hpp"
#include "data_ids.hpp"
#include <cstdio>

// morning - evening: 0 - 1000
// night: 1000 - 1250

inline constexpr uint32_t morning = 0;
inline constexpr uint32_t evening = 1000;
inline constexpr uint32_t day_lenght = 1250;

struct skill_ids {
	dcon::skill_id cooking;
};

struct ai_state {
	dcon::activity_id weapon_repair;
	dcon::activity_id shopping;
	dcon::activity_id getting_food;
	dcon::activity_id prepare_food;
	dcon::activity_id working;
};

struct ai_personality {
	dcon::ai_model_id hunter;
};

struct game_state {
	dcon::data_container data;
	uint32_t time;


	dcon::commodity_id potion;
	dcon::commodity_id coins;
	dcon::commodity_id potion_material;
	dcon::commodity_id raw_food;
	dcon::commodity_id prepared_food;
	dcon::commodity_id weapon_service;


	dcon::building_model_id inn;
	dcon::building_model_id shop;
	dcon::building_model_id shop_weapon;

	skill_ids skills;
	ai_state ai;
	ai_personality personality;
};

static int market_activity = 0;

void transaction(game_state& game, dcon::character_id A, dcon::character_id B, dcon::commodity_id C, float amount) {
	assert(amount >= 0.f);
	auto i_A = game.data.character_get_inventory(A, C);
	auto i_B = game.data.character_get_inventory(B, C);

	assert(i_A >= amount);
	game.data.character_set_inventory(A, C, i_A - amount);
	game.data.character_set_inventory(B, C, i_B + amount);

	market_activity += 1;
}

void delayed_transaction(game_state& game, dcon::character_id A, dcon::character_id B, dcon::commodity_id C, float amount) {
	auto delayed = game.data.get_delayed_transaction_by_transaction_pair(A, B);
	if (delayed) {
		auto mul = 1;
		if (A == game.data.delayed_transaction_get_members(delayed, 1)) {
			mul = -1;
		}
		auto debt = game.data.delayed_transaction_get_balance(delayed, C);
		game.data.delayed_transaction_set_balance(delayed, C, debt + (float)mul * amount);
	} else {
		delayed = game.data.force_create_delayed_transaction(A, B);
		game.data.delayed_transaction_set_balance(delayed, C, amount);
	}
}

void hunt(game_state& game, dcon::character_id cid) {
	auto timer = game.data.character_get_action_timer(cid);
	if (timer > 4) {
		auto weapon = game.data.character_get_weapon_quality(cid);
		auto bonus = int(weapon / 0.2);
		auto food = game.data.character_get_inventory(cid, game.raw_food);
		game.data.character_set_inventory(cid, game.raw_food, food + 1.f + (float)bonus);
		game.data.character_set_action_timer(cid, 0);
		game.data.character_set_action_type(cid, {});
	} else {
		game.data.character_set_action_timer(cid, timer + 1);
	}
	game.data.character_set_hp(cid, game.data.character_get_hp(cid) - 1);
	auto quality = game.data.character_get_weapon_quality(cid);
	game.data.character_set_weapon_quality(cid, quality * 0.99f);
}

void repair_weapon(game_state& game, dcon::character_id cid, dcon::character_id master) {
	auto timer = game.data.character_get_action_timer(cid);
	auto weapon_repair_price = game.data.character_get_price_belief_sell(master, game.weapon_service);
	if (timer == 0) {
		game.data.character_set_action_timer(cid, timer + 1);
		transaction(game, cid, master, game.coins, weapon_repair_price);
		game.data.character_set_price_belief_sell(master, game.weapon_service, weapon_repair_price * 1.05f);
	} else if (timer > 4) {
		auto quality = game.data.character_get_weapon_quality(cid);
		game.data.character_set_weapon_quality(cid, quality + 0.1f);
		game.data.character_set_action_timer(cid, 0.f);
		game.data.character_set_action_type(cid, {});
		game.data.delete_guest(game.data.character_get_guest(cid));
	} else {
		game.data.character_set_action_timer(cid, timer + 1);
	}
}

void make_potion(game_state& game, dcon::character_id cid) {
	auto material = game.data.character_get_inventory(cid, game.potion_material);
	assert(material >= 1.f);
	auto timer = game.data.character_get_action_timer(cid);
	if (timer > 10) {
		auto potion = game.data.character_get_inventory(cid, game.potion);
		game.data.character_set_inventory(cid, game.potion_material, material - 1.f);
		game.data.character_set_inventory(cid, game.potion, potion + 1.f);
		game.data.character_set_action_timer(cid, 0);
		game.data.character_set_action_type(cid, {});
	} else {
		game.data.character_set_action_timer(cid, timer + 1);
	}
}

void prepare_food(game_state& game, dcon::character_id cid) {
	auto material = game.data.character_get_inventory(cid, game.raw_food);
	assert(material >= 1.f);
	auto timer = game.data.character_get_action_timer(cid);
	if (timer > 1) {
		auto result = game.data.character_get_inventory(cid, game.prepared_food);
		// auto skill = game.data.character_get_skills(cid, )
		auto skill_bonus = (float)(int)(game.data.character_get_skills(cid, game.skills.cooking) / 0.1);
		game.data.character_set_inventory(cid, game.raw_food, material - 1.f);
		game.data.character_set_inventory(cid, game.prepared_food, result + 1.f + skill_bonus);
		game.data.character_set_action_timer(cid, 0);
		game.data.character_set_action_type(cid, {});
	} else {
		game.data.character_set_action_timer(cid, timer + 1);
	}
}

void gather_potion_material(game_state& game, dcon::character_id cid) {
	auto timer = game.data.character_get_action_timer(cid);
	if (timer > 3) {
		auto count = game.data.character_get_inventory(cid, game.potion_material);
		game.data.character_set_inventory(cid, game.potion_material, count + 1.f);
		game.data.character_set_action_timer(cid, 0);
		game.data.character_set_action_type(cid, {});
	} else {
		game.data.character_set_action_timer(cid, timer + 1);
	}
}

void eat(game_state& game, dcon::character_id cid) {
	auto food = game.data.character_get_inventory(cid, game.prepared_food);
	auto hunger = game.data.character_get_hunger(cid);
	if (food >= 1.f) {
		game.data.character_set_inventory(cid, game.prepared_food, food - 1);
		game.data.character_set_hunger(cid, hunger - 50);

		auto hp = game.data.character_get_hp(cid);
		game.data.character_set_hp(cid, hp + 5);
	}
}

void drink_potion(game_state& game, dcon::character_id cid) {
	auto potions = game.data.character_get_inventory(cid, game.potion);
	auto hp = game.data.character_get_hp(cid);
	auto hp_max = game.data.character_get_hp_max(cid);

	if (hp * 2 < hp_max && potions >= 1.f) {
		game.data.character_set_inventory(cid, game.potion, potions - 1);
		game.data.character_set_hp(cid, hp + 20);
	}
}



enum class skill {
	travel, archery, swordsmanship, gathering, alchemy, trade
};

namespace ai {

namespace triggers {


bool desire_weapon_repair(game_state& game, dcon::character_id cid, dcon::character_id master) {
	if (game.data.character_get_weapon_quality(cid) > 2.f) {
		return false;
	}

	auto coins = game.data.character_get_inventory(cid, game.coins);
	auto weapon_repair_price = game.data.character_get_price_belief_sell(master, game.weapon_service);
	if (weapon_repair_price * 3.f > coins) {
		return false;
	}

	return true;
}


}


namespace update_activity {

void hunter(game_state& game, dcon::character_id cid) {
	auto x = game.data.character_get_x(cid);
	auto y = game.data.character_get_y(cid);
	auto coins = game.data.character_get_inventory(cid, game.coins);
	auto action = game.data.character_get_action_type(cid);
	auto ai_type = game.data.character_get_ai_type(cid);

	assert(ai_type == game.personality.hunter);

	auto favourite_weapons_shop = game.data.character_get_favourite_shop_weapons(cid);
	auto favourite_weapons_shop_owner = game.data.building_get_owner_from_ownership(favourite_weapons_shop);
	auto weapon_repair_price = game.data.character_get_price_belief_sell(favourite_weapons_shop_owner, game.weapon_service);

	if (!action) {
		auto loot  = game.data.character_get_inventory(cid, game.raw_food);
		auto loot_target = game.data.ai_model_get_stockpile_target(ai_type, game.raw_food);

		if (loot_target > 3) {
			game.data.character_set_action_type(cid, game.ai.shopping);
		} else if (
			ai::triggers::desire_weapon_repair(game, cid, favourite_weapons_shop_owner)
		) {
			game.data.character_set_action_type(cid, game.ai.weapon_repair);
		} else if (
			game.data.character_get_hunger(cid) > 200
			&& game.data.character_get_inventory(cid, game.raw_food) >= 1.f
		) {
			game.data.character_set_action_type(cid, game.ai.prepare_food);
		} else {
			game.data.character_set_action_type(cid, game.ai.working);
		}
	}

	auto guest_in = game.data.character_get_guest_location_from_guest(cid);

	if (action == game.ai.weapon_repair) {
		if (
			ai::triggers::desire_weapon_repair(game, cid, favourite_weapons_shop_owner)
		) {
			auto target_x = game.data.building_get_tile_x(favourite_weapons_shop);
			auto target_y = game.data.building_get_tile_y(favourite_weapons_shop);

			auto dx = (float) target_x - x;
			auto dy = (float) target_y - y;
			auto distance = abs(dx) + abs(dy);

			if (guest_in == favourite_weapons_shop) {
				repair_weapon(game, cid, favourite_weapons_shop_owner);
			} else if (guest_in) {
				game.data.delete_guest(game.data.character_get_guest(cid));
			} else if (distance < 2){
				game.data.force_create_guest(cid, favourite_weapons_shop);
			} else {
				if (dx != 0) {
					game.data.character_set_x(cid, x + dx / abs(dx));
				}
				if (dy != 0) {
					game.data.character_set_y(cid, y + dy / abs(dy));
				}
			}
		} else {
			game.data.character_set_action_timer(cid, 0);
			game.data.character_set_action_type(cid, {});
		}
		return;
	} else if (action == game.ai.prepare_food) {
		prepare_food(game, cid);
		return;
	} else if (action == game.ai.working) {
		hunt(game, cid);
		return;
	}
	game.data.character_set_action_timer(cid, 0);
}

}

}

game_state game {};
int main(int argc, char const* argv[]) {

	printf("%d", argc);
	if (argc > 0 ) {
		printf("%s", argv[0]);
	}

	game.data.character_resize_skills(256);
	game.data.character_resize_price_belief_buy(256);
	game.data.character_resize_price_belief_sell(256);
	game.data.character_resize_inventory(256);
	game.data.ai_model_resize_stockpile_target(256);
	game.data.delayed_transaction_resize_balance(256);

	game.ai.getting_food = game.data.create_activity();
	game.ai.shopping = game.data.create_activity();
	game.ai.weapon_repair = game.data.create_activity();
	game.ai.working = game.data.create_activity();
	game.ai.prepare_food = game.data.create_activity();

	game.coins = game.data.create_commodity();
	game.potion_material = game.data.create_commodity();
	game.potion = game.data.create_commodity();
	game.raw_food = game.data.create_commodity();
	game.prepared_food = game.data.create_commodity();
	game.weapon_service = game.data.create_commodity();

	game.skills.cooking = game.data.create_skill();

	game.inn = game.data.create_building_model();
	game.shop = game.data.create_building_model();
	game.shop_weapon = game.data.create_building_model();

	auto human = game.data.create_kind();
	auto rat = game.data.create_kind();

	{
		game.personality.hunter = game.data.create_ai_model();
		game.data.ai_model_set_stockpile_target(game.personality.hunter, game.potion, 7);
		game.data.ai_model_set_stockpile_target(game.personality.hunter, game.prepared_food, 3);
	}


	auto shopkeeper_model = game.data.create_ai_model();
	game.data.for_each_commodity([&](auto commodity){
		if (commodity == game.coins) {
			return;
		}
		game.data.ai_model_set_stockpile_target(shopkeeper_model, commodity, 10);
	});

	auto innkeeper_model = game.data.create_ai_model();
	game.data.ai_model_set_stockpile_target(innkeeper_model, game.raw_food, 10);
	game.data.ai_model_set_stockpile_target(innkeeper_model, game.prepared_food, 5);
	game.data.ai_model_set_stockpile_target(innkeeper_model, game.potion, 1);


	auto alchemist_model = game.data.create_ai_model();
	game.data.ai_model_set_stockpile_target(alchemist_model, game.prepared_food, 5);
	game.data.ai_model_set_stockpile_target(alchemist_model, game.potion_material, 10);


	auto weapon_master_model = game.data.create_ai_model();
	game.data.ai_model_set_stockpile_target(weapon_master_model, game.prepared_food, 5);
	game.data.ai_model_set_stockpile_target(weapon_master_model, game.potion, 1);


	auto herbalist_model = game.data.create_ai_model();
	game.data.ai_model_set_stockpile_target(herbalist_model, game.prepared_food, 5);
	game.data.ai_model_set_stockpile_target(herbalist_model, game.potion, 1);


	auto deliverer_model = game.data.create_ai_model();
	game.data.ai_model_set_stockpile_target(deliverer_model, game.prepared_food, 5);
	game.data.ai_model_set_stockpile_target(deliverer_model, game.potion, 1);


	// {
	// 	auto deliverer = game.data.create_character();
	// 	game.data.character_set_hp(deliverer, 100);
	// 	game.data.character_set_hp_max(deliverer, 100);
	// }
	for (int i = 0; i < 4; i++) {
		auto hunter = game.data.create_character();
		game.data.character_set_hp(hunter, 100);
		game.data.character_set_hp_max(hunter, 100);
		game.data.character_set_ai_type(hunter, game.personality.hunter);
		game.data.character_set_weapon_quality(hunter, 1.f);
	}

	{
		auto inn = game.data.create_building();
		game.data.building_set_tile_x(inn, 0);
		game.data.building_set_tile_y(inn, 0);
		game.data.building_set_building_model(inn, game.inn);

		auto innkeeper = game.data.create_character();
		game.data.character_set_inventory(innkeeper, game.coins, 10);
		game.data.character_set_ai_type(innkeeper, innkeeper_model);
		game.data.character_set_skills(innkeeper, game.skills.cooking, 0.3f);

		game.data.force_create_ownership(innkeeper, inn);
	}
	{
		auto shop = game.data.create_building();
		game.data.building_set_tile_x(shop, 3);
		game.data.building_set_tile_y(shop, 3);
		game.data.building_set_building_model(shop, game.shop);
		auto shop_owner = game.data.create_character();
		game.data.character_set_inventory(shop_owner, game.coins, 100);
		game.data.character_set_ai_type(shop_owner, shopkeeper_model);

		game.data.force_create_ownership(shop_owner, shop);
	}
	{
		auto shop_weapons = game.data.create_building();
		game.data.building_set_tile_x(shop_weapons, 3);
		game.data.building_set_tile_y(shop_weapons, 0);
		game.data.building_set_building_model(shop_weapons, game.shop_weapon);
		auto weapon_master = game.data.create_character();
		game.data.character_set_inventory(weapon_master, game.coins, 10);
		game.data.character_set_ai_type(weapon_master, weapon_master_model);

		game.data.force_create_ownership(weapon_master, shop_weapons);
	}

	{
		auto alchemist = game.data.create_character();
		game.data.character_set_inventory(alchemist, game.coins, 100);
		game.data.character_set_ai_type(alchemist, alchemist_model);
	}

	{
		auto alchemist = game.data.create_character();
		game.data.character_set_inventory(alchemist, game.coins, 100);
		game.data.character_set_ai_type(alchemist, alchemist_model);
	}

	{
		auto herbalist = game.data.create_character();
		game.data.character_set_ai_type(herbalist, herbalist_model);
	}

	game.data.for_each_character([&](auto cid) {
		game.data.for_each_commodity([&](auto commodity) {
			game.data.character_set_price_belief_buy(cid, commodity, 1.f);
			game.data.character_set_price_belief_sell(cid, commodity, 1.f);
		});

		// select initial favorite shops
		game.data.for_each_building([&](auto candidate_building){
			auto candidate = game.data.building_get_owner_from_ownership(candidate_building);
			dcon::ai_model_id model = game.data.character_get_ai_type(candidate);
			if (model == innkeeper_model) {
				game.data.character_set_favourite_inn(cid, candidate_building);
			}
			if (model == weapon_master_model) {
				game.data.character_set_favourite_shop_weapons(cid, candidate_building);
			}
			if (model == shopkeeper_model) {
				game.data.character_set_favourite_shop(cid, candidate_building);
			}
		});
	});

	FILE* file_handle;
	file_handle = fopen("statistics/market_activity.txt", "w");

	fprintf(file_handle, "activity,debt\n");
	for (int tick = 0; tick < 10000; tick++) {
		auto total_debt = 0.f;
		game.data.for_each_delayed_transaction([&](auto transaction) {
			total_debt += std::abs(game.data.delayed_transaction_get_balance(transaction, game.coins));
		});
		fprintf(file_handle, "%d,%f\n", market_activity, total_debt);
		// market_activity = 0;

		// AI update
		game.data.for_each_character([&](auto cid) {
			auto model = game.data.character_get_ai_type(cid);
			if (model == game.personality.hunter) {
				ai::update_activity::hunter(game, cid);
			} else if (model == alchemist_model) {
				auto potion_price = game.data.character_get_price_belief_sell(cid, game.potion);
				auto potion_material_cost = game.data.character_get_price_belief_buy(cid, game.potion_material);
				if (
					game.data.character_get_inventory(cid, game.potion_material) >= 1.f
					&& potion_price > potion_material_cost * 2.f
				) {
					make_potion(game, cid);
				}

				auto timer = game.data.character_get_action_timer(cid);
				game.data.character_set_action_timer(cid, timer + 1);
			} else if (model == herbalist_model) {
				gather_potion_material(game, cid);

				auto timer = game.data.character_get_action_timer(cid);
				game.data.character_set_action_timer(cid, timer + 1);
			} else if (model == innkeeper_model) {
				if (game.data.character_get_inventory(cid, game.raw_food) >= 1.f) {
					printf("make food\n");
					prepare_food(game, cid);
				}

				auto timer = game.data.character_get_action_timer(cid);
				game.data.character_set_action_timer(cid, timer + 1);
			}
			auto hunger = game.data.character_get_hunger(cid);
			game.data.character_set_hunger(cid, hunger + 1);
		});

		// trade:

		// ai logic would be very simple:
		// sell things you don't desire yourself
		// buy things you desire and miss

		// currently we can buy things only from the favourite shop:
		for (int round = 0; round < 3; round++) {
			game.data.for_each_character([&](auto cid) {
				game.data.for_each_commodity([&](auto commodity) {
					if (commodity == game.coins) {
						return;
					}

					auto shop = game.data.character_get_favourite_shop(cid);
					auto shop_owner = game.data.building_get_owner_from_ownership(shop);
					if (commodity == game.prepared_food) {
						auto shop_owner = game.data.character_get_favourite_inn(cid);
					}
					if (cid == shop_owner) {
						return;
					}
					// auto desire = game.data.character_get_hunger(cid, commodity);
					auto ai = game.data.character_get_ai_type(cid);
					auto target = game.data.ai_model_get_stockpile_target(ai, commodity);
					auto inventory = game.data.character_get_inventory(cid, commodity);
					auto in_stock = game.data.character_get_inventory(shop_owner, commodity);
					auto coins = game.data.character_get_inventory(cid, game.coins);
					auto desired_price_buy = game.data.character_get_price_belief_buy(cid, commodity);
					auto desired_price_sell = game.data.character_get_price_belief_sell(cid, commodity);
					auto coins_shop = game.data.character_get_inventory(shop_owner, game.coins);
					auto price_shop_sell = game.data.character_get_price_belief_sell(shop_owner, commodity);
					auto price_shop_buy = game.data.character_get_price_belief_buy(shop_owner, commodity);


					if (target > inventory) {
						printf("I need this? %d %f %f %f\n", commodity.index(), desired_price_buy, price_shop_sell, in_stock );
						if (desired_price_buy >= price_shop_sell && in_stock >= 1.f && coins >= price_shop_sell) {
							printf("I am buying\n");
							transaction(game, shop_owner, cid, commodity, 1.f);
							transaction(game, cid, shop_owner, game.coins, price_shop_sell);
						} else if (desired_price_buy >= price_shop_sell && coins >= price_shop_sell) {
							printf("I am ordering\n");
							auto delayed = game.data.get_delayed_transaction_by_transaction_pair(cid, shop_owner);
							bool already_indebted = false;
							if (delayed) {
								auto A = game.data.delayed_transaction_get_members(delayed, 0);
								auto B = game.data.delayed_transaction_get_members(delayed, 1);
								auto debt = game.data.delayed_transaction_get_balance(delayed, commodity);
								auto mult = 1.f;
								if (A != shop_owner) {
									mult = -1.f;
								}
								if (debt * mult > 3) {
									already_indebted = true;
								}
							}
							if (!already_indebted) {
								delayed_transaction(game, shop_owner, cid, commodity, 1.f);
								transaction(game, cid, shop_owner, game.coins, price_shop_sell);
							} else {
								printf("But I have already ordered a lot\n");
							}
						}
					}

					if (target < inventory) {
						printf("I do not need this? %d %f %f %f\n", commodity.index(), desired_price_sell, price_shop_buy, in_stock );

						if (price_shop_buy >= desired_price_sell && inventory >= 1.f && coins_shop >= price_shop_buy) {
							printf("I am selling\n");
							transaction(game, cid, shop_owner, commodity, 1.f);
							transaction(game, shop_owner, cid, game.coins, price_shop_buy);
						} else if (price_shop_buy >= desired_price_sell && inventory >= 1.f) {
							printf("I am selling for promise of future payment\n");
							transaction(game, cid, shop_owner, commodity, 1.f);
							delayed_transaction(game, shop_owner, cid, game.coins, price_shop_buy);
						}
					}
				});
			});
		}

		// fulfill promises:
		game.data.for_each_delayed_transaction([&](auto delayed) {
			auto A = game.data.delayed_transaction_get_members(delayed, 0);
			auto B = game.data.delayed_transaction_get_members(delayed, 1);

			game.data.for_each_commodity([&](auto commodity) {
				auto debt = game.data.delayed_transaction_get_balance(delayed, commodity);
				if (debt > 0) {
					auto inv = game.data.character_get_inventory(A, commodity);
					if (inv >= 1) {
						transaction(game, A, B, commodity, 1.f);
						delayed_transaction(game, A, B, commodity, -1.f);
					}
				} else if (debt < 0) {
					auto inv = game.data.character_get_inventory(B, commodity);
					if (inv >= 1) {
						transaction(game, B, A, commodity, 1.f);
						delayed_transaction(game, B, A, commodity, -1.f);
					}
				}
			});
		});

		game.data.for_each_character([&](auto cid) {
			if (game.data.character_get_hunger(cid) > 50.f) {
				eat(game, cid);
			}
			drink_potion(game, cid);
		});

		// event: if commodity is not selling well: reduce sell price:

		if (tick % 5 == 0) {
			game.data.for_each_character([&](auto cid) {
				if (game.data.character_get_ai_type(cid) == weapon_master_model) {
					auto cost = game.data.character_get_price_belief_sell(cid, game.weapon_service);
					game.data.character_set_price_belief_sell(cid, game.weapon_service, cost * 0.99f);
				}
				game.data.for_each_commodity([&](auto commodity) {
					if (commodity == game.coins) {
						return;
					}

					auto inventory = game.data.character_get_inventory(cid, commodity);
					auto ai = game.data.character_get_ai_type(cid);
					auto target = game.data.ai_model_get_stockpile_target(ai, commodity);

					if (inventory > target * 2) {
						auto price_shop_sell = game.data.character_get_price_belief_sell(cid, commodity);
						game.data.character_set_price_belief_sell(cid, commodity, 0.00001f + price_shop_sell * 0.99f);

						auto price_shop_buy = game.data.character_get_price_belief_buy(cid, commodity);
						game.data.character_set_price_belief_buy(cid, commodity, 0.00001f + price_shop_buy * 0.95f);
					}
					// if something is spoiling, we want to get rid of it
					if (inventory >= 20) {
						// spoilage
						game.data.character_set_inventory(cid, commodity, inventory - (float)(int)(inventory / 20));

						auto price_shop_sell = game.data.character_get_price_belief_sell(cid, commodity);
						game.data.character_set_price_belief_sell(cid, commodity, 0.00001f + price_shop_sell * 0.99f);

						auto price_shop_buy = game.data.character_get_price_belief_buy(cid, commodity);
						game.data.character_set_price_belief_buy(cid, commodity, 0.00001f + price_shop_buy * 0.95f);
					}

					auto coins = game.data.character_get_inventory(cid, game.coins);

					if (inventory < target) {
						auto price_shop_buy = game.data.character_get_price_belief_buy(cid, commodity);
						game.data.character_set_price_belief_buy(cid, commodity, std::min(coins + 10.f,  price_shop_buy * 1.01f));
						auto price_shop_sell = game.data.character_get_price_belief_sell(cid, commodity);

						if (game.data.character_get_ai_type(cid) == shopkeeper_model) {
							game.data.character_set_price_belief_sell(cid, commodity,  price_shop_buy * 1.1f);
						} else {
							game.data.character_set_price_belief_sell(cid, commodity, std::min(coins + 10.f,  price_shop_sell * 1.05f));
						}
					}
				});
			});
		}

		game.data.for_each_character([&](auto cid) {
			dcon::character_fat_id fcid = dcon::fatten(game.data, cid);
			printf("%d,(%d),(%d) weapon: %f, coins: %f, hunger: %f\n",
				cid.index(), fcid.get_ai_type().id.index(), fcid.get_action_type().id.index(),
				fcid.get_weapon_quality(), fcid.get_inventory(game.coins),
				fcid.get_hunger()
			);
		});

		printf("FOOD:\n");
		game.data.for_each_character([&](auto cid) {
			dcon::character_fat_id fcid = dcon::fatten(game.data, cid);
			printf("%f %f %f\n", fcid.get_inventory(game.prepared_food), fcid.get_price_belief_buy(game.prepared_food), fcid.get_price_belief_sell(game.prepared_food));
		});

		printf("RAW FOOD:\n");
		game.data.for_each_character([&](auto cid) {
			dcon::character_fat_id fcid = dcon::fatten(game.data, cid);
			printf("%f %f %f\n", fcid.get_inventory(game.raw_food), fcid.get_price_belief_buy(game.raw_food), fcid.get_price_belief_sell(game.raw_food));
		});

		printf("POTIONS:\n");
		game.data.for_each_character([&](auto cid) {
			dcon::character_fat_id fcid = dcon::fatten(game.data, cid);
			printf("%f %f %f\n", fcid.get_inventory(game.potion), fcid.get_price_belief_buy(game.potion), fcid.get_price_belief_sell(game.potion));
		});

		printf("WEAPON:\n");
		game.data.for_each_character([&](auto cid) {
			dcon::character_fat_id fcid = dcon::fatten(game.data, cid);
			printf("%f %f %f\n", fcid.get_inventory(game.weapon_service), fcid.get_price_belief_buy(game.weapon_service), fcid.get_price_belief_sell(game.weapon_service));
		});

		printf("\n------------\n");
	}
}