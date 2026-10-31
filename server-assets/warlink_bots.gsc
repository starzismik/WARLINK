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
    LogPrint("WARLINK_BOTS_READY\n");
    level thread warlink_manage_bots();
    level thread warlink_connections();
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
        }
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
