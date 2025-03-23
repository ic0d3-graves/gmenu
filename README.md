gmenu
gmenu is a lightweight, customizable, graphical menu application for X11 environments, written in C. It provides a right-click context menu with support for submenus, color-coded labels, transparency, and command execution. Ideal for desktop environments or window managers lacking a built-in menu system.
Features
Right-Click Activation: Trigger the menu with a right-click (Button3) on the root window.

Customizable Appearance: Configurable colors (foreground, background, selected states), transparency, border width, and font via .gmenurc.

Submenu Support: Nested menu items with dynamic positioning and hover-based expansion.

Color-Coded Labels: Use <color='#RRGGBB'>text</color> syntax in .gmenu_items for styled text segments.

Command Execution: Execute shell commands or launch applications, with optional terminal support (default: alacritty -e).

Dynamic Sizing: Automatically adjusts menu width based on content, with padding and icon support.

Separators: Add visual breaks in the menu using ___ in the items file.

Transparency: Supports alpha blending for both normal and hover states.

Logging: Debug output written to a log file (if enabled).

Dependencies
To build and run gmenu, ensure the following libraries and tools are installed:
X11 Libraries:
libX11 (Xlib for core X functionality)

libXrender (rendering extension for transparency)

Xft:
libXft (font rendering with FreeType)

Development Tools:
gcc (or another C compiler)

make (for building with the provided Makefile)

pkg-config (to resolve Xft dependencies during compilation)

Optional:
alacritty (default terminal emulator; can be customized via TERMINAL define)

Installation on Debian/Ubuntu
bash

sudo apt update
sudo apt install libx11-dev libxrender-dev libxft-dev build-essential pkg-config alacritty

Installation on Fedora
bash

sudo dnf install libX11-devel libXrender-devel libXft-devel make gcc pkg-config alacritty

Installation on Arch Linux
bash

sudo pacman -S libx11 libxrender libxft base-devel pkgconf alacritty

Font Requirements
gmenu uses the Xft library for font rendering and requires a valid font specification in .gmenurc. By default, it falls back to "fixed" if no font is specified or if the specified font is unavailable.
Font Format: Use Xft-compatible font names (e.g., "SF Pro Display:style=Regular:size=12").

Checking Available Fonts: Use fc-list (from fontconfig) to list installed fonts:
bash

fc-list | grep "SF Pro"

Installation: Install desired fonts via your package manager or manually (e.g., sudo apt install fonts-sf-pro if available).

If the configured font is not found, gmenu will attempt to use "fixed". Ensure at least a basic X11 font is available.
Build and Installation
Clone the Repository:
bash

git clone https://github.com/yourusername/gmenu.git
cd gmenu

Build:
bash

make

Install (optional, installs to /usr/bin/):
bash

sudo make install

Clean (optional):
bash

make clean

Configuration
.gmenurc
Place this file in ~/.config/gmenu/.gmenurc to customize the menu's appearance. Example:
ini

foreground = #338292
background = #000000
selected_fg = #060810
selected_bg = #338292
font = SF Pro Display:style=Regular:size=12
transparency = 0.5
hover_transparency = 0.7
border_width = 1
border_color = #5E6E9B
submenu_offset = -10
icon_left_padding = 5
icon_right_padding = 2

Colors are in #RRGGBB hex format.

transparency and hover_transparency range from 0.0 (fully transparent) to 1.0 (opaque).

.gmenu_items
Define menu items in ~/.config/gmenu/.gmenu_items. Syntax:
label = command (top-level item)

== label = command (submenu item under the last top-level item)

___ (separator)

<color='#RRGGBB'>text</color> (colored text within labels)

Example:

􀈖 File Manager = thunar
<color='#0078D7'>􀎭</color> Microsoft Edge = microsoft-edge-dev --new-window
___
􀣌 System Utilities
== 􀐙 EasyEffects = easyeffects
== 􀚁 Pacman Update = bash -i -c 'alacritty -e sudo pacman -Syyuu'

Usage
Run:
bash

./gmenu

Or, if installed:
bash

gmenu

Interact:
Right-click anywhere on the root window to show the menu.

Hover over items to highlight them or open submenus.

Left-click an item to execute its command.

Click outside the menu to dismiss it.

Capabilities
Menu Structure: Supports up to 100 top-level items (MAX_ITEMS) and unlimited submenu items (limited by memory).

Text Rendering: Renders UTF-8 text with Xft, including emoji and special characters.

Dynamic Positioning: Adjusts menu and submenu positions to fit within screen boundaries.

Command Flexibility: Runs commands in the background (&) and supports terminal-based execution for bash commands.

Extensibility: Easily extendable via configuration files without recompiling.

Limitations
Requires a 32-bit TrueColor visual for transparency (most modern systems support this).

No built-in icon support beyond text-based glyphs (e.g., emoji).

Logging is enabled only if log_file is set (not implemented in this version).

Contributing
Feel free to fork this repository, submit issues, or send pull requests. Suggestions for improving usability, performance, or adding features (e.g., image icons, keybindings) are welcome!
License
This project is released under the MIT License (LICENSE). See the LICENSE file for details.

