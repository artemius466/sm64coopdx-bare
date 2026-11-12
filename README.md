![sm64coopdx Logo](textures/segment2/custom_coopdx_logo.rgba32.png)

sm64coopdx is an online multiplayer project for the Super Mario 64 PC port that synchronizes all entities and every level for multiple players. The project was started by the Coop Deluxe Team. The purpose is to actively maintain and improve, but also continue sm64ex-coop, created by djoslin0. More features, customization, and power to the Lua API allow modders and players to enjoy Super Mario 64 more than ever!

Feel free to report bugs or contribute to the project.

## About This Fork

This is `sm64coopdx-bare`, a fork of sm64coopdx that removes unused code. Its goal is a smaller, easier to maintain codebase: dead US/PC-only paths, unused runtime buffers, and other unreachable code are stripped out, while the multiplayer, Lua API, and modding features remain intact.

One of the reasons for this fork is to make working with AI agents simpler. By reducing the amount of garbage in the codebase, there is less irrelevant context to sift through, which reduces token usage and makes agent-assisted development more efficient.

## Initial Goal (Accomplished)
Create a mod for the PC port where multiple people can play together online.

Unlike previous multiplayer projects, this one synchronizes enemies and events. This allows players to interact with the same world at the same time.

Interestingly enough though, the goal of the project has slowly evolved over time from simply just making a Super Mario 64 multiplayer mod to constantly maintaining and improving the project (notably the Lua API.)

## Documentation

sm64coopdx is moddable via Lua, similar to Roblox and Garry's Mod's Lua APIs. To get started, click [here](docs/lua/lua.md) to see the Lua documentation. If you want to contribute to the repo, you can view the C documentation [here](docs/c/c.md).

## Wiki
The wiki is made using GitHub's wiki feature, you can go to the wiki tab or click [here](https://github.com/coop-deluxe/sm64coopdx/wiki).

## Community
We have an official Discord server open to the public [here](https://discord.gg/TJVKHS4).
