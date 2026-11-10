# Configuration window

Basic and Advanced titles align to the left of their header area. XIV1.0 configuration uses muted silver control rims, thinner layered borders and subdued window/separator decoration. Hover, keyboard/controller focus and error states remain distinct.

[Setup](setup.md) introduces profiles and the core modules on a fresh install; `/xiuiplus setup` reopens it without resetting your layout.

[Basic settings and HUD scale](hud-scaling.md) documents Basic/Advanced modes, the Classic/XIV whole-HUD presets, Global Scale, element scale controls and edit grips. The General sidebar category is now Global.

Settings open with **Edit Mode OFF**. Only live modules appear; hover one for quick editing or select its settings to preview it. The footer's dot toggle turns ON previews of all enabled modules. Turning it OFF or reopening settings clears previews. Hidden modules also lose their mouse and controller edit controls.

Module destinations expand under the selected category on the left instead of occupying a top button row. Inventory reveals Satchel, Dots, Equipment, Gil and Treasure Pool; Extras reveals its individual modules. Expansion and collapse use a short height/slide animation. Long sidebars scroll independently by wheel, scrollbar or controller focus. Leaving categories have no active child targets. Selecting an icon-only category with destinations expands its labels so those destinations remain reachable.

Global's topics use a second navigation column. Appearance opens window styling directly; Global Text and Config Settings sit alongside Enabled Modules, Grid, Input and Visibility. Config Settings brings together menu scale, configuration text styling and Follow moved module settings. Native UI is a main category for HUD windows and Menus and dialogs, including overhead names and native macros. Extras contains Controller Focus and HP Feedback. Attribution opens from the top-right button, with tabs for its credit pages. Previews, settings, hover help and saved selections use the same destinations. Pages without top controls start directly below the header. The minus header button collapses Configuration to its title bar; plus restores it.

Visual settings share one Appearance entry with indented categories such as Size & Position, Window, Bar Style, Colors and Effects. Text and functional topics stay separate. The topic and destination lists share their geometry with pointer, keyboard/controller, hover help and sounds. Original saved navigation and option IDs are retained.

The Pet tabs select the pet type whose settings you are editing, with only the current tab selected. Avatar / spirit, Jug pet, Charmed pet, Wyvern and Automaton retain independent settings; Pet Target opens the shared target settings. Visibility is controlled by Enabled and Show this pet type. `xiui_config_input_smoke --pet-tabs-only` checks exclusive selection through pointer and controller actions.

**Global → Config Settings → Config text size (px)** adjusts configuration text live from 8–18 pixels, next to Config font. The default is 14 pixels; headings and help text keep their relative sizes. This saves with profiles, resets with defaults and leaves HUD text sizes independent.

**Global → Config Settings → Menu Scale (%)** enlarges the entire configuration window in 25% steps from 100% to 200%, including controls, spacing, text, menus and dialogs. Basic and Advanced share the same scale. Config text size remains an independent preference. The window keeps its position when space permits and fits the game viewport; smaller screens may limit the effective scale. The choice stays local to this installation across profiles, HUD presets and resets. HUD elements retain their own scales and positions.

The header's input-mode button shows a keyboard or controller icon. Hover it to see the configured mode and Auto's current presentation; click it for the full **Auto**, **Keyboard & Mouse**, and **Controller** options. Selecting a presentation retains the existing input behavior.

The color picker's HEX field is editable. Click it or focus it with the controller and Confirm, then type or paste `#RRGGBB` or `#RRGGBBAA` (the `#` is optional; alpha is last). Enter/Tab or controller Confirm applies a valid value; Escape/Cancel discards the text edit while keeping the picker open. Invalid input leaves the previous color intact. The separate **Copy** button retains clipboard export.

Click the **search icon** in the header to animate it into the **Search settings...** input, or press **Ctrl+F** to open and focus it directly. It uses the shared softly rounded, inset input surface and native control palette without an outline stroke. A subtle underline and caret indicate typing focus. The shortcut appears as a smaller hint at the right when empty; the header collapses back to the icon when results close, retaining the query for next time. An **X** appears once you type; click it to clear the query and keep typing focus. Search combines words across setting names, their category/topic/element path and help text. For example, **party hp**, **party hp height**, or **party hp font** finds the corresponding Party controls, including HP-specific bar and text elements.

Results use a darker native-themed panel for readability. Click a result or use Up/Down and Enter to open it. Configuration selects its category, view, topic and bar/text element, scrolls to the control and briefly highlights it. Search does not apply presets, alter values or enable disabled modules. Results follow the current configuration's conditional controls; changing a mode and reopening search refreshes the catalog.

Mouse wheel and Page Up/Down browse longer result lists. Escape closes search; Ctrl+F selects the current query for replacement. Controller **Utilities → Search** opens the same field: use a keyboard to type, directions to select/page results, Confirm to open, Actions to clear and Cancel to close. Foreground loss closes search and releases typing ownership, preserving ordinary chat drafts.

Focused checks: `xiui_config_input_smoke --settings-search-only` verifies matching, index isolation, HP bar/text navigation, disabled-category inspection, input release, icon expansion/collapse, clear-button focus retention and shared mouse/controller actions. The 6,541-entry default catalog built in about 19 ms in the focused check; live appearance remains for Cody to review.

Use the arrow at the top-left to collapse the category sidebar to icons. Hover an icon to see its category name; click it as usual. Click the arrow again to restore labels. The saved choice reloads with the configuration layout, and the settings area immediately uses the freed width.

Basic and Advanced share a footer with the profile dropdown, create and actions on the left and the settings-mode button on the right. Collapsing the sidebar retains this full profile dropdown. Profile menus open above the footer. Navigation, option rows and scrollbars reserve its space, and minimum window height keeps category/topic buttons readable above it.

Closing and reopening retains the chosen Basic/Advanced mode, window placement and last Advanced page with its scroll position. This navigation state also survives reloads and stays local rather than entering shared profiles. Focused check: `xiui_config_input_smoke --config-memory-only`.

Controller **Utilities → Collapse sidebar to icons / Expand sidebar** uses the same action. Category names remain available through focus help, and reflow preserves the selected page and focused setting. Focused verification: `xiui_config_input_smoke --config-sidebar-only` and `--config-resize-only`.

Drag an edge or corner to resize Configuration. The lower-right grip uses the current theme. The chosen position and dimensions save with the normal configuration layout.

Settings reflow between one and two columns according to the available content width, including space taken by topic navigation and the scrollbar. Font sizes remain unchanged. Related width/height and X/Y controls stay adjacent in the settings order. Preset cards, element selectors and custom list editors use the available content area too.

Rendering, pointer controls, scrolling and controller navigation use the same layout and option slots. Reflow retains the current page and keeps the focused setting visible. Minimum dimensions preserve usable controls and readable navigation; the window stays inside the game viewport.

Configuration keeps authored dimensions and a screen-space top-left position. `config/config_window_scale.hpp` and the runtime's Config transform helpers map drawing, pointer coordinates and popup viewports together. Resize deltas convert once; dragging remains in screen coordinates. Configuration draw scopes restore the preceding transform so HUD editing, module menus and action workspaces retain their own scale.

Keyboard users can start resizing with **Alt+F8**, adjust with the arrow keys, press **Space** for one-pixel steps, and use **Enter** to save or **Escape** to cancel. Controller users can select **Resize** in Configuration's Utilities region and use directional input, Actions for fine steps, Confirm and Cancel. Both routes use the same size constraints as dragging. Keyboard resizing also works when controller enumeration is disabled.

Resizing changes only the configuration window's layout. It does not apply presets or change HUD preferences.

## First-load layout fitting

The shipped layout was authored at **2560×1440**. XIUI+ records that reference in `layout.viewport`, waits for a valid Direct3D game viewport, and fits untouched default positions to the player's window. It preserves the stacked chat offsets and central bottom HUD alignment; other default elements retain their relative edge placement. Window dimensions and text styling remain as configured, with normal bounds checks for smaller windows.

Missing position settings receive fitted defaults. Moved modules retain their saved positions, including when another resolution is detected; old layouts without viewport metadata remain unchanged. Untouched defaults can be fitted again from the original preset rather than scaling the previous result repeatedly. Saves and portable profiles include their reference viewport.

For a first-load layout already saved in a clamped state by an older build, `/xiuiplus layout defaults` restores **positions only**, fitted to the current game window. Current appearance, sizes, actions, bindings and visibility choices are retained. Ordinary reloads preserve customized placement.

Focused check: `xiui_config_input_smoke --viewport-layout-only config/XIUIplus-defaults.ini`.
