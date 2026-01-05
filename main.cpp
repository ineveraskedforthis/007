#include "data.hpp"
#include "data_ids.hpp"
#include <cstdio>

// morning - evening: 0 - 1000
// night: 1000 - 1250

inline constexpr uint32_t morning = 0;
inline constexpr uint32_t evening = 1000;
inline constexpr uint32_t day_lenght = 1250;

struct game_state {
	dcon::data_container data;
	uint32_t time;


	dcon::commodity_id potion;
	dcon::commodity_id coins;
	dcon::commodity_id potion_material;
	dcon::commodity_id raw_food;
	dcon::commodity_id prepared_food;
	dcon::commodity_id weapon_service;

	dcon::activity_id weapon_repair;

	dcon::building_model_id inn;
	dcon::building_model_id shop;
	dcon::building_model_id shop_weapon;
};

void hunt(game_state& game, dcon::character_id cid) {
	auto timer = game.data.character_get_action_timer(cid);
	if (timer > 4) {
		auto weapon = game.data.character_get_weapon_quality(cid);
		auto bonus = int(weapon / 0.2);
		auto food = game.data.character_get_inventory(cid, game.raw_food);
		game.data.character_set_inventory(cid, game.raw_food, food + 1.f + (float)bonus);
		game.data.character_set_action_timer(cid, 0);
		game.data.character_set_action_type(cid, {});
	}
	game.data.character_set_hp(cid, game.data.character_get_hp(cid) - 1);
	auto quality = game.data.character_get_weapon_quality(cid);
	game.data.character_set_weapon_quality(cid, quality * 0.99f);
}

void repair_weapon(game_state& game, dcon::character_id cid) {
	auto timer = game.data.character_get_action_timer(cid);
	if (timer > 4) {
		auto quality = game.data.character_get_weapon_quality(cid);
		game.data.character_set_weapon_quality(cid, quality + 0.1f);
		game.data.character_set_action_timer(cid, 0.f);
		game.data.character_set_action_type(cid, {});
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
	}
}

void prepare_food(game_state& game, dcon::character_id cid) {
	auto material = game.data.character_get_inventory(cid, game.raw_food);
	assert(material >= 1.f);
	auto timer = game.data.character_get_action_timer(cid);
	if (timer > 1) {
		auto result = game.data.character_get_inventory(cid, game.prepared_food);
		game.data.character_set_inventory(cid, game.raw_food, material - 1.f);
		game.data.character_set_inventory(cid, game.prepared_food, result + 1.f);
		game.data.character_set_action_timer(cid, 0);
		game.data.character_set_action_type(cid, {});
	}
}

void gather_potion_material(game_state& game, dcon::character_id cid) {
	auto timer = game.data.character_get_action_timer(cid);
	if (timer > 3) {
		auto count = game.data.character_get_inventory(cid, game.potion_material);
		game.data.character_set_inventory(cid, game.potion_material, count + 1.f);
		game.data.character_set_action_timer(cid, 0);
		game.data.character_set_action_type(cid, {});
	}
}

void eat(game_state& game, dcon::character_id cid) {
	auto food = game.data.character_get_inventory(cid, game.prepared_food);
	auto hunger = game.data.character_get_desire(cid, game.prepared_food);
	if (food >= 1.f) {
		game.data.character_set_inventory(cid, game.prepared_food, food - 1);
		game.data.character_set_desire(cid, game.prepared_food, hunger - 10);

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

void transaction(game_state& game, dcon::character_id A, dcon::character_id B, dcon::commodity_id C, float amount) {
	assert(amount >= 0.f);
	auto i_A = game.data.character_get_inventory(A, C);
	auto i_B = game.data.character_get_inventory(B, C);

	assert(i_A >= amount);
	game.data.character_set_inventory(A, C, i_A - amount);
	game.data.character_set_inventory(B, C, i_B + amount);
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

enum class skill {
	travel, archery, swordsmanship, gathering, alchemy, trade
};

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
	game.data.character_resize_desire(256);
	game.data.ai_model_resize_stockpile_target(256);
	game.data.delayed_transaction_resize_balance(256);

	game.coins = game.data.create_commodity();
	game.potion_material = game.data.create_commodity();
	game.potion = game.data.create_commodity();
	game.raw_food = game.data.create_commodity();
	game.prepared_food = game.data.create_commodity();
	game.weapon_service = game.data.create_commodity();

	game.inn = game.data.create_building_model();
	game.shop = game.data.create_building_model();
	game.shop_weapon = game.data.create_building_model();

	auto human = game.data.create_kind();
	auto rat = game.data.create_kind();

	auto hunter_model = game.data.create_ai_model();
	game.data.ai_model_set_stockpile_target(hunter_model, game.potion, 7);
	game.data.ai_model_set_stockpile_target(hunter_model, game.prepared_food, 3);


	auto shopkeeper_model = game.data.create_ai_model();
	game.data.for_each_commodity([&](auto commodity){
		if (commodity == game.coins) {
			return;
		}
		game.data.ai_model_set_stockpile_target(shopkeeper_model, commodity, 10);
	});

	auto innkeeper_model = game.data.create_ai_model();
	game.data.ai_model_set_stockpile_target(innkeeper_model, game.potion, 7);
	game.data.ai_model_set_stockpile_target(innkeeper_model, game.raw_food, 10);



	auto alchemist_model = game.data.create_ai_model();
	game.data.ai_model_set_stockpile_target(alchemist_model, game.prepared_food, 5);
	game.data.ai_model_set_stockpile_target(alchemist_model, game.potion_material, 10);


	auto weapon_master_model = game.data.create_ai_model();
	game.data.ai_model_set_stockpile_target(weapon_master_model, game.prepared_food, 5);

	auto herbalist_model = game.data.create_ai_model();
	game.data.ai_model_set_stockpile_target(herbalist_model, game.prepared_food, 5);


	{
		auto hunter = game.data.create_character();
		game.data.character_set_hp(hunter, 100);
		game.data.character_set_hp_max(hunter, 100);
		game.data.character_set_ai_type(hunter, hunter_model);
		game.data.character_set_weapon_quality(hunter, 1.f);
	}
	{
		auto hunter = game.data.create_character();
		game.data.character_set_hp(hunter, 100);
		game.data.character_set_hp_max(hunter, 100);
		game.data.character_set_ai_type(hunter, hunter_model);
		game.data.character_set_weapon_quality(hunter, 1.f);
	}
	{
		auto hunter = game.data.create_character();
		game.data.character_set_hp(hunter, 100);
		game.data.character_set_hp_max(hunter, 100);
		game.data.character_set_ai_type(hunter, hunter_model);
		game.data.character_set_weapon_quality(hunter, 1.f);
	}
	{
		auto hunter = game.data.create_character();
		game.data.character_set_hp(hunter, 100);
		game.data.character_set_hp_max(hunter, 100);
		game.data.character_set_ai_type(hunter, hunter_model);
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
		game.data.character_set_inventory(alchemist, game.coins, 10);
		game.data.character_set_ai_type(alchemist, alchemist_model);
	}

	{
		auto alchemist = game.data.create_character();
		game.data.character_set_inventory(alchemist, game.coins, 10);
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
		game.data.for_each_character([&](auto candidate){
			dcon::ai_model_id model = game.data.character_get_ai_type(candidate);
			if (model == innkeeper_model) {
				game.data.character_set_favourite_inn(cid, candidate);
			}
			if (model == weapon_master_model) {
				game.data.character_set_favourite_shop_weapons(cid, candidate);
			}
			if (model == shopkeeper_model) {
				game.data.character_set_favourite_shop(cid, candidate);
			}
		});
	});



	game.weapon_repair = game.data.create_activity();

	for (int tick = 0; tick < 1000; tick++) {
		game.data.for_each_character([&](auto cid) {
			auto model = game.data.character_get_ai_type(cid);
			if (model == hunter_model) {

				auto favourite_shop = game.data.character_get_favourite_shop_weapons(cid);

				auto weapon_repair_price = game.data.character_get_price_belief_sell(favourite_shop, game.weapon_service);
				auto coins = game.data.character_get_inventory(cid, game.coins);
				if (
					game.data.character_get_weapon_quality(cid) < 1.5f
					&& weapon_repair_price * 3.f < coins
					&& !game.data.character_get_action_type(cid)
				) {
					game.data.character_set_action_type(cid, game.weapon_repair);
					transaction(game, cid, favourite_shop, game.coins, weapon_repair_price);
					game.data.character_set_price_belief_sell(favourite_shop, game.weapon_service, weapon_repair_price * 1.05f);
					printf("I will repair weapons\n");
				}

				if (game.data.character_get_action_type(cid) == game.weapon_repair) {
					repair_weapon(game, cid);
				} else {
					hunt(game, cid);
				}
			} else if (model == alchemist_model) {
				if (game.data.character_get_inventory(cid, game.potion_material) > 1.f) {
					make_potion(game, cid);
				}
			} else if (model == herbalist_model) {
				gather_potion_material(game, cid);
			} else if (model == innkeeper_model) {
				if (game.data.character_get_inventory(cid, game.raw_food) > 1.f) {
					prepare_food(game, cid);
				}
			}
			auto timer = game.data.character_get_action_timer(cid);
			game.data.character_set_action_timer(cid, timer + 1);
			auto hunger = game.data.character_get_desire(cid, game.prepared_food);
			game.data.character_set_desire(cid, game.prepared_food, hunger + 1);
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

					auto shop_owner = game.data.character_get_favourite_shop(cid);
					if (commodity == game.prepared_food) {
						auto shop_owner = game.data.character_get_favourite_inn(cid);
					}
					if (cid == shop_owner) {
						return;
					}
					// auto desire = game.data.character_get_desire(cid, commodity);
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
			if (game.data.character_get_desire(cid, game.prepared_food) > 10.f) {
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
			printf("%d,(%d) weapon: %f, coins: %f, hunger: %f\n",
				cid.index(), fcid.get_action_type().id.index(),
				fcid.get_weapon_quality(), fcid.get_inventory(game.coins),
				fcid.get_desire(game.prepared_food)
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