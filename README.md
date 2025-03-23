# gmenu

gmenu is a lightweight, X11-based menu launcher designed for quick access to your favorite applications and system utilities. It reads configuration and menu item files from the user’s configuration directory and supports dynamic, multi-level menus with customizable styling.
![image](https://github.com/user-attachments/assets/01c58e76-7b5c-441e-b565-a2e93953555a)


## Project Dependencies

- **X11 Libraries:**  
  gmenu uses the core X11 libraries for window management and event handling.
  
- **Xrender:**  
  The Xrender extension is used to manage advanced graphical effects, including transparency and smooth rendering.
  
- **Xft:**  
  For improved text rendering, gmenu relies on the Xft library, which offers support for anti-aliased fonts.

- **Standard Unix Libraries:**  
  Additional dependencies include standard libraries for input/output, string handling, time functions, and process management.

## Font Requirements

- **Xft-Compatible Fonts:**  
  gmenu requires an Xft-compatible font to render menu text.  
  - A default fallback (typically "fixed") is used if the specified font is not available.
  - The configuration file allows you to set any preferred font. For example, you might use "SF Pro Display:style=Regular:size=12" or any other font installed on your system.

## Documented Capabilities

- **Dynamic Menu Generation:**  
  Reads menu items from a user-provided file, allowing you to define top-level entries and submenus.
  
- **Customizable Appearance:**  
  - Configure foreground and background colors.
  - Set transparency levels for both normal and hover states.
  - Adjust padding, border width, and submenu offsets.
  
- **Color-Coded Menu Items:**  
  Supports inline color markup in menu labels for enhanced visual feedback.
  
- **Submenus:**  
  Allows the creation of hierarchical menus, making it easy to group related commands under a single entry.
  
- **Command Execution:**  
  Executes specified shell commands when a menu item is selected, including support for launching terminal-based commands.
  
- **Logging:**  
  All actions and errors are logged to a file, which helps in troubleshooting and understanding menu interactions.

- **Mouse Interaction:**  
  Uses X11 event handling to provide intuitive mouse-based selection, hover effects, and automatic submenu creation.

# .gmenurc Configuration File

The `.gmenurc` file is the configuration file for gmenu that allows you to customize its appearance and behavior without changing the source code. It is typically located in your configuration directory (e.g., `~/.config/gmenu/`).

## Available Settings

- **foreground**  
  Specifies the default text color for menu items. The color is provided in hexadecimal format (e.g., `#338292`).

- **background**  
  Sets the background color of the menu using a hexadecimal color code (e.g., `#000000`).

- **selected_fg**  
  Defines the text color for a highlighted or selected menu item (e.g., `#060810`).

- **selected_bg**  
  Sets the background color for the selected menu item (e.g., `#338292`).

- **font**  
  Specifies the Xft-compatible font used to render the menu text. For example, `SF Pro Display:style=Regular:size=12` determines the font style and size.

- **transparency**  
  Determines the transparency level for non-selected menu items. Values range from 0.0 (completely transparent) to 1.0 (fully opaque), e.g., `0.5`.

- **border_width**  
  Sets the width of the border around the menu in pixels (e.g., `1`).

- **border_color**  
  Specifies the border color using a hexadecimal color code (e.g., `#5E6E9B`).

- **submenu_offset**  
  Adjusts the horizontal offset for submenus relative to the main menu. A positive value shifts the submenu to the right, while a negative value shifts it to the left (e.g., `-10`).

- **icon_left_padding**  
  Sets the padding on the left side of any icons within the menu (e.g., `5`).

- **icon_right_padding**  
  Sets the padding on the right side of icons (e.g., `2`).

- **hover_transparency**  
  Defines the transparency level for a hovered (selected) menu item. Like the normal transparency, it ranges from 0.0 to 1.0 (e.g., `0.7`).

## Usage

When gmenu starts, it reads the `.gmenurc` file to apply your custom settings. This makes it easy to adjust the visual style and layout of the menu to better match your desktop environment or personal preferences.


# .gmenu_items File

The `.gmenu_items` file defines the menu structure and the commands that gmenu executes. It allows you to create top-level menu items, submenus, separators, and apply inline color formatting to menu labels.

## Structure and Syntax

- **Top-Level Items**  
  Each line for a top-level menu item should include a label and its corresponding command, separated by " = ". For example:  
  `File Manager = thunar`  
  This creates a menu item labeled "File Manager" that launches the Thunar file manager when selected.

- **Submenu Items**  
  To create submenu items, begin the line with `==`. These lines are associated with the most recent top-level item. For example:  
  `== VS Code = code`  
  This indicates that "VS Code" is a submenu item for the preceding parent item.

- **Separators**  
  Use a line containing three underscores `___` to insert a visual separator between groups of menu items. Separators help organize the menu into logical sections.

- **Inline Color Markup**  
  Menu labels can include inline color formatting by wrapping text in `<color='#hexcode'>...</color>` tags. For example:  
  `<color='#0078D7'>Icon</color> Microsoft Edge = microsoft-edge-dev --new-window`  
  This applies the specified color to the "Icon" part of the label, while the rest of the text remains in the default color.

## Example

A snippet from a `.gmenu_items` file might look like this:

􀈖 File Manager = thunar  
<color='#0078D7'>􀎭</color> Microsoft Edge = microsoft-edge-dev --new-window  
<color='#007ACC'>􀙅</color> VS Code = code  
􀎭 Qutebrowser = qutebrowser --target window https://google.com  
___  
􀉟 Coin Market = qutebrowser --target window https://coinmarketcap.com  

- The first four lines define top-level menu items with labels and commands.
- The separator (`___`) divides the menu into distinct sections.
- Additional items can be added after the separator.

## Customization

By editing the `.gmenu_items` file, you can:
- Adjust the labels and commands to match your workflow.
- Group related commands into submenus.
- Use inline color markup to highlight or differentiate parts of the labels.

The file is read at runtime by gmenu, so any changes you make will be reflected the next time the menu is loaded.

  

## Installation & Usage

- **Compilation:**  
  gmenu can be compiled using `gcc` with the necessary X11, Xrender, and Xft libraries linked. The provided Makefile automates this process.

- **Configuration:**  
  - A configuration file (e.g., `.gmenurc`) allows you to customize fonts, colors, transparency, and other visual parameters.
  - The menu items file (e.g., `.gmenu_items`) defines the commands to be executed for each menu entry.

- **Execution:**  
  After compiling and installing, gmenu can be run as a standalone menu launcher or integrated into your desktop environment's workflow.

# Detailed Build Instructions for gmenu on Arch, Debian, and Fedora

Follow the steps below for your specific distribution. Each section lists the required commands and processes, including cleaning previous builds, compiling, and installing gmenu.

## Arch-Based Distributions

1. **Update the System**  
   Run: `sudo pacman -Syu`

2. **Install Dependencies**  
   Ensure you have the required libraries and tools by running:  
   `sudo pacman -S xorg-server xorg-apps libx11 libxrender libxft gcc make`

3. **Clone the Repository**  
   Navigate to your desired source directory and clone gmenu:  
   `git clone https://github.com/yourusername/gmenu.git`  
   Then change into the repository:  
   `cd gmenu`

4. **Clean Previous Builds (if needed)**  
   Run: `make clean`

5. **Compile the Project**  
   Build gmenu by running: `make`

6. **Install gmenu**  
   To install the executable system-wide, run: `sudo make install`

## Debian-Based Distributions

1. **Update the Package Lists**  
   Run: `sudo apt update`

2. **Install Dependencies**  
   Install the required development packages by running:  
   `sudo apt install libx11-dev libxrender-dev libxft-dev build-essential`

3. **Clone the Repository**  
   Clone the gmenu repository into your chosen directory by running:  
   `git clone https://github.com/yourusername/gmenu.git`  
   Then change into the directory:  
   `cd gmenu`

4. **Clean Previous Builds (if needed)**  
   Run: `make clean`

5. **Compile the Project**  
   Build gmenu by running: `make`

6. **Install gmenu**  
   Install the binary system-wide by running: `sudo make install`

## Fedora-Based Distributions

1. **Update the System**  
   Run: `sudo dnf update`

2. **Install Dependencies**  
   Install the necessary development packages by running:  
   `sudo dnf install libX11-devel libXrender-devel libXft-devel gcc make`

3. **Clone the Repository**  
   Clone the repository by running:  
   `git clone https://github.com/yourusername/gmenu.git`  
   Then change into the cloned directory:  
   `cd gmenu`

4. **Clean Previous Builds (if needed)**  
   Run: `make clean`

5. **Compile the Project**  
   Build gmenu by running: `make`

6. **Install gmenu**  
   Install the compiled binary by running: `sudo make install`

