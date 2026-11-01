#include maps\mp\bots\_bots;

/*
WARLINK dedicated bot controller.
Engine bot APIs verified against HorizonMW and the bot examples published by
DoktorSAS / Kalitos / BoxOfMysteriez. No player stats or ranks are modified.
*/

init()
{
    if (!getdvarint("dedicated") || !getdvarint("warlink_managed_server"))
        return;
    precacheitem("h2_l118a_mp_l118ascope");
    level.warlink_snipers = ["h2_l118a_mp_l118ascope"];
    level.warlink_sniper_next = 0;
    level thread warlink_build_sniper_catalog();
    LogPrint("WARLINK_BOTS_READY\n");
    level thread warlink_manage_bots();
    level thread warlink_connections();
    level thread warlink_team_choices();
    level thread warlink_team_requests();
    level thread warlink_moderation();
    level thread warlink_banner_tests();
    level thread warlink_xp_controller();
    LogPrint("WARLINK_MESSAGES_READY\n");
}

warlink_manage_bots()
{
    level endon("game_ended");
    wait 5;
    last_difficulty = "";
    for (;;)
    {
        if (!isdefined(level.players))
        {
            wait 1;
            continue;
        }
        humans = 0;
        bots = [];
        foreach (player in level.players)
        {
            if (isBot(player))
                bots[bots.size] = player;
            else
                humans++;
        }
        target = getdvarint("warlink_bot_count");
        slots = getdvarint("party_maxplayers") - humans;
        if (target > slots) target = slots;
        if (target < 0) target = 0;
        if (target > 17) target = 17;

        if (bots.size < target)
            spawn_bots(1, "autoassign");
        else if (bots.size > target)
            executecommand("kickClient " + bots[0] getentitynumber());

        difficulty = getdvar("warlink_bot_difficulty");
        if (difficulty != last_difficulty)
        {
            LogPrint("WARLINK_BOT_DIFFICULTY " + difficulty + "\n");
            last_difficulty = difficulty;
        }
        foreach (bot in bots)
        {
            skill = difficulty;
            if (skill == "mixed")
                skill = common_scripts\utility::random(["recruit", "regular", "hardened", "veteran"]);
            if (skill != "recruit" && skill != "regular" && skill != "hardened" && skill != "veteran")
                skill = "regular";
            bot maps\mp\bots\_bots_util::bot_set_difficulty(skill, undefined);
            if (!isdefined(bot.warlink_loadout_started))
            {
                bot.warlink_loadout_started = true;
                bot thread warlink_loadout_spawns();
                bot thread warlink_death_drops();
            }
            bot warlink_loadout();
        }
        if (getdvarint("warlink_bot_inspect"))
        {
            setdvar("warlink_bot_inspect", "0");
            drops = 0;
            foreach (sniper in level.warlink_snipers) drops += getentarray("weapon_" + sniper, "classname").size;
            LogPrint("WARLINK_DROPPED_WEAPONS " + drops + "\n");
            foreach (bot in bots)
            {
                if (bot.sessionstate != "playing") continue;
                held = bot getcurrentweapon();
                LogPrint("WARLINK_BOT_WEAPON slot=" + bot getentitynumber() + " held=" + held + " ammo=" + bot getcurrentweaponclipammo() + "\n");
            }
        }
        if (getdvarint("warlink_bot_balance")) warlink_balance_bots();
        wait 2;
    }
}

warlink_connections()
{
    level endon("game_ended");
    for (;;)
    {
        level waittill("connected", player);
        if (!isBot(player))
        {
            player thread warlink_welcome();
            player thread warlink_announcements();
            player thread warlink_xp_observer();
        }
    }
}

warlink_welcome()
{
    self endon("disconnect");
    first_spawn = true;
    for (;;)
    {
        self waittill("spawned_player");
        wait 1;
        if (getdvar("warlink_bot_weapons") == "sniper") self enableweaponpickup();
        if (first_spawn || getdvarint("warlink_welcome_respawn"))
        {
            text = getdvar("warlink_welcome");
            discord = getdvar("warlink_discord");
            if (text != "") self warlink_display_message(text);
            if (discord != "")
            {
                wait 3;
                self warlink_display_message("Discord : " + discord);
            }
            LogPrint("WARLINK_WELCOME_SENT\n");
        }
        first_spawn = false;
    }
}

warlink_announcements()
{
    self endon("disconnect");
    for (;;)
    {
        interval = getdvarint("warlink_announcement_interval");
        if (interval < 10) interval = 10;
        wait interval;
        if (getdvarint("warlink_announcement_interval") > 0)
        {
            text = getdvar("warlink_announcement");
            if (text != "") self warlink_display_message(text);
        }
    }
}

warlink_display_message(text)
{
    position = getdvar("warlink_message_position");
    if (position == "center")
    {
        self iprintlnbold(text);
        return;
    }
    if (position != "left")
    {
        self iprintln(text);
        return;
    }
    if (isdefined(self.warlink_banner)) self.warlink_banner destroy();
    banner = newclienthudelem(self);
    self.warlink_banner = banner;
    banner.horzalign = "left";
    banner.vertalign = "middle";
    banner.alignx = "left";
    banner.aligny = "middle";
    banner.x = 20;
    banner.y = -60;
    banner.fontscale = 1.5;
    banner.alpha = 1;
    banner settext(text);
    self thread warlink_clear_banner(banner);
}

warlink_clear_banner(banner)
{
    self endon("disconnect");
    wait 6;
    if (isdefined(banner)) banner destroy();
}

warlink_loadout_spawns()
{
    self endon("disconnect");
    for (;;)
    {
        self waittill("spawned_player");
        self.warlink_weapon_mode = undefined;
        self.warlink_sniper_weapon = undefined;
        wait 0.5;
        self warlink_loadout();
    }
}

warlink_loadout()
{
    if (self.sessionstate != "playing") return;
    mode = getdvar("warlink_bot_weapons");
    if (mode == "") mode = "mixed";
    if (isdefined(self.warlink_weapon_mode) && self.warlink_weapon_mode == mode)
    {
        if (mode != "sniper" || (isdefined(self.warlink_sniper_weapon) && self hasweapon(self.warlink_sniper_weapon) && self getcurrentweapon() == self.warlink_sniper_weapon)) return;
    }
    previous = self.warlink_weapon_mode;
    self.warlink_weapon_mode = mode;
    self enableweapons();
    self enableweaponswitch();
    self enableoffhandweapons();
    self allowmelee(true);
    if (mode == "mixed")
    {
        self enableweaponpickup();
        if (isdefined(previous) && previous != "mixed") self suicide();
        return;
    }
    if (mode == "sniper")
    {
        if (!isdefined(self.warlink_sniper_weapon) || previous != "sniper")
        {
            self.warlink_sniper_weapon = level.warlink_snipers[level.warlink_sniper_next];
            level.warlink_sniper_next = (level.warlink_sniper_next + 1) % level.warlink_snipers.size;
            if (isdefined(self.warlink_last_sniper) && self.warlink_sniper_weapon == self.warlink_last_sniper && level.warlink_snipers.size > 1)
            {
                self.warlink_sniper_weapon = level.warlink_snipers[level.warlink_sniper_next];
                level.warlink_sniper_next = (level.warlink_sniper_next + 1) % level.warlink_snipers.size;
            }
            self.warlink_last_sniper = self.warlink_sniper_weapon;
        }
    }
    self disableweaponpickup();
    self takeallweapons();
    self disableoffhandweapons();
    weapon = "none";
    if (mode == "none")
    {
        self disableweapons();
        self allowmelee(false);
    }
    else
    {
        weapon = "h2_l118a_mp_l118ascope";
        if (mode == "sniper") weapon = self.warlink_sniper_weapon;
        self giveweapon(weapon);
        self switchtoweaponimmediate(weapon);
        self.currentweaponatspawn = weapon;
        self.lastdroppableweapon = weapon;
        self disableweaponswitch();
        if (mode == "sniper") self givemaxammo(weapon);
        else
        {
            self setweaponammoclip(weapon, 0);
            self setweaponammostock(weapon, 0);
        }
    }
    held = self getcurrentweapon();
    LogPrint("WARLINK_LOADOUT_APPLIED " + mode + " held=" + held + " selected=" + weapon + "\n");
}

warlink_balance_bots()
{
    if (getdvar("g_gametype") == "dm" || getdvar("g_gametype") == "gun") return;
    allies = [];
    axis = [];
    foreach (player in level.players)
    {
        if (!isdefined(player.pers["team"])) continue;
        if (player.pers["team"] == "allies") allies[allies.size] = player;
        if (player.pers["team"] == "axis") axis[axis.size] = player;
    }
    if (abs(allies.size - axis.size) < 2) return;
    larger = allies;
    target = "axis";
    if (axis.size > allies.size) { larger = axis; target = "allies"; }
    foreach (bot in larger)
    {
        if (!isBot(bot)) continue;
        bot.switching_teams = true;
        bot.joining_team = target;
        bot.leaving_team = bot.pers["team"];
        if (bot.sessionstate == "playing") bot suicide();
        bot maps\mp\gametypes\_menus::addtoteam(target);
        bot maps\mp\gametypes\_menus::endrespawnnotify();
        LogPrint("WARLINK_BOT_TEAM_BALANCED\n");
        return;
    }
}

warlink_banner_tests()
{
    level endon("game_ended");
    for (;;)
    {
        wait 1;
        if (!getdvarint("warlink_test_banner")) continue;
        setdvar("warlink_test_banner", "0");
        sent = 0;
        foreach (player in level.players)
        {
            if (isBot(player)) continue;
            player warlink_display_message("^2WARLINK ^7| ^3TEST BANDEAU MILIEU GAUCHE");
            sent++;
        }
        LogPrint("WARLINK_BANNER_TEST_SENT " + sent + "\n");
    }
}

/*
Expose the stock team selector to human clients. Server CFG alone cannot set
ui_allow_teamchange because that client dvar is cheat protected.
*/
warlink_team_choices()
{
    level endon("game_ended");
    for (;;)
    {
        wait 1;
        allowed = getdvarint("warlink_allow_team_change");
        if (getdvar("g_gametype") == "dm" || getdvar("g_gametype") == "gun") allowed = 0;
        if (allowed) setdvar("scr_teambalance", "0");
        foreach (player in level.players)
        {
            if (isBot(player)) continue;
            if (isdefined(player.warlink_team_choice) && player.warlink_team_choice == allowed) continue;
            player.warlink_team_choice = allowed;
            player setclientdvar("ui_allow_teamchange", allowed);
            if (allowed) player.dont_auto_balance = true;
            else player.dont_auto_balance = undefined;
            LogPrint("WARLINK_TEAM_CHOICE " + player getentitynumber() + " " + allowed + "\n");
        }
    }
}

warlink_move_to_team(team)
{
    if (self.pers["team"] == team) return;
    self.switching_teams = true;
    self.joining_team = team;
    self.leaving_team = self.pers["team"];
    if (self.sessionstate == "playing") self suicide();
    self maps\mp\gametypes\_menus::addtoteam(team);
    self maps\mp\gametypes\_menus::endrespawnnotify();
}

warlink_team_requests()
{
    level endon("game_ended");
    setdvar("warlink_team_request", "");
    for (;;)
    {
        wait 0.5;
        request = getdvar("warlink_team_request");
        if (request == "") continue;
        setdvar("warlink_team_request", "");
        parts = strtok(request, "|");
        if (parts.size != 4) continue;
        result = parts[3] + " rejected";
        team = parts[2];
        if (!getdvarint("warlink_allow_team_change") || (team != "allies" && team != "axis") || getdvar("g_gametype") == "dm" || getdvar("g_gametype") == "gun")
        {
            setdvar("warlink_team_result", result);
            continue;
        }
        foreach (player in level.players)
        {
            if (isBot(player) || player getentitynumber() != int(parts[0])) continue;
            if (tolower(player getguid()) != parts[1]) continue;
            if (player.pers["team"] != team)
            {
                other = "allies";
                if (team == "allies") other = "axis";
                foreach (bot in level.players)
                {
                    if (!isBot(bot) || bot.pers["team"] != team) continue;
                    bot warlink_move_to_team(other);
                    break;
                }
                player warlink_move_to_team(team);
            }
            player.dont_auto_balance = true;
            if (player.pers["team"] == team) result = parts[3] + " ok";
            LogPrint("WARLINK_HUMAN_TEAM_CHANGED " + player getentitynumber() + " " + team + "\n");
        }
        setdvar("warlink_team_result", result);
    }
}

/* Server-side exclusions remain active when the launcher is closed. */
warlink_moderation()
{
    level endon("game_ended");
    setdvar("warlink_mod_request", "");
    for (;;)
    {
        wait 0.5;
        now = gettimeutc();
        setdvar("warlink_moderation_utc", now);
        foreach (player in level.players)
        {
            if (isBot(player)) continue;
            guid = tolower(player getguid());
            until = getdvarint("warlink_ban_" + guid);
            if (until == -1 || until > now)
            {
                LogPrint("WARLINK_EXCLUSION_ENFORCED " + guid + "\n");
                kick(player getentitynumber(), "WARLINK : exclusion de ce serveur");
            }
        }
        request = getdvar("warlink_mod_request");
        if (request == "") continue;
        setdvar("warlink_mod_request", "");
        parts = strtok(request, "|");
        if (parts.size != 6) continue;
        result = parts[3] + " rejected";
        foreach (player in level.players)
        {
            if (isBot(player) || player getentitynumber() != int(parts[0])) continue;
            if (tolower(player getguid()) != parts[1]) continue;
            action = parts[2];
            if (action == "kick" || action == "temporary" || action == "ban")
            {
                if (action != "kick") setdvar("warlink_ban_" + parts[1], parts[5]);
                result = parts[3] + " ok";
                setdvar("warlink_mod_result", result);
                kick(player getentitynumber(), parts[4]);
                LogPrint("WARLINK_MODERATION " + action + " " + parts[1] + "\n");
            }
        }
        setdvar("warlink_mod_result", result);
    }
}

warlink_build_sniper_catalog()
{
    bases = ["h1_barrett", "h1_dragunov", "h1_m21", "h1_m40a3", "h1_remington700", "h1_febsnp", "h1_junsnp"];
    rows = tablegetrowcount("mp/customweapons.csv");
    for (i = 0; i < rows; i++)
    {
        if (tablelookupbyrow("mp/customweapons.csv", i, 1) != "weapon_sniper") continue;
        base = tablelookupbyrow("mp/customweapons.csv", i, 0);
        duplicate = false;
        foreach (existing in bases) if (existing == base) duplicate = true;
        if (!duplicate) bases[bases.size] = base;
    }
    catalog = [];
    foreach (base in bases)
    {
        if (getsubstr(base, 0, 3) == "h1_")
            weapon = base + "_mp";
        else
            weapon = base + "_mp_" + getsubstr(base, 3, base.size) + "scope";
        if (weaponclass(weapon) != "sniper" && getsubstr(base, 0, 3) == "h1_")
        {
            alternatives = [base + "_mp_a#none_f#base", getsubstr(base, 3, base.size) + "_mp"];
            foreach (alternate in alternatives) if (weaponclass(alternate) == "sniper") { weapon = alternate; break; }
        }
        if (weaponclass(weapon) != "sniper")
        {
            LogPrint("WARLINK_SNIPER_REJECTED " + base + " " + weapon + "\n");
            continue;
        }
        precacheitem(weapon);
        catalog[catalog.size] = weapon;
        LogPrint("WARLINK_SNIPER_VALIDATED " + base + " " + weapon + "\n");
    }
    if (catalog.size) level.warlink_snipers = catalog;
    level.warlink_sniper_next = 0;
    setdvar("warlink_sniper_count", level.warlink_snipers.size);
}

warlink_death_drops()
{
    self endon("disconnect");
    for (;;)
    {
        self waittill("death");
        if (getdvar("warlink_bot_weapons") != "sniper" || !isdefined(self.warlink_sniper_weapon)) continue;
        weapon = self.warlink_sniper_weapon;
        location = self.origin;
        wait 0.1;
        dropped = undefined;
        source = "existing";
        foreach (item in getentarray("weapon_" + weapon, "classname"))
        {
            if (distance(item.origin, location) < 64) { dropped = item; break; }
        }
        if (!isdefined(dropped)) { dropped = self dropitem(weapon); source = "created"; }
        if (isdefined(dropped))
        {
            dropped thread warlink_drop_cleanup();
            LogPrint("WARLINK_SNIPER_DROPPED " + weapon + " class=" + dropped.classname + " source=" + source + "\n");
        }
    }
}

warlink_drop_cleanup()
{
    wait 45;
    if (isdefined(self)) self delete();
}

warlink_xp_controller()
{
    level endon("game_ended");
    previous = -1;
    for (;;)
    {
        wait 0.25;
        requested = getdvarint("warlink_xp_multiplier");
        if (requested != 1 && requested != 2 && requested != 4) requested = 1;
        if (requested != previous || !isdefined(level.xpScale) || level.xpScale != requested)
        {
            old_scale = "undefined";
            if (isdefined(level.xpScale)) old_scale = level.xpScale;
            level.xpScale = requested;
            setdvar("scr_xpscale", requested);
            setdvar("warlink_xp_effective", requested);
            LogPrint("WARLINK_XP_APPLIED requested=" + requested + " old=" + old_scale + " effective=" + level.xpScale + "\n");
            previous = requested;
        }
        if (getdvarint("warlink_xp_inspect"))
        {
            setdvar("warlink_xp_inspect", "0");
            foreach (p in level.players)
            {
                if (isBot(p)) continue;
                xp = p getplayerdata(common_scripts\utility::getstatsgroup_ranked(), "experience");
                LogPrint("WARLINK_XP_OBSERVED " + p getentitynumber() + " xp=" + xp + " scale=" + level.xpScale + "\n");
            }
        }
    }
}

warlink_xp_observer()
{
    self endon("disconnect");
    wait 3;
    previous = self getplayerdata(common_scripts\utility::getstatsgroup_ranked(), "experience");
    for (;;)
    {
        wait 1;
        current = self getplayerdata(common_scripts\utility::getstatsgroup_ranked(), "experience");
        if (current != previous)
        {
            LogPrint("WARLINK_XP_GAIN slot=" + self getentitynumber() + " before=" + previous + " after=" + current + " delta=" + (current - previous) + " scale=" + level.xpScale + "\n");
            previous = current;
        }
    }
}
