# Configuration window

The Pet tabs select the pet type whose settings you are editing, with only the current tab selected. Avatar / spirit, Jug pet, Charmed pet, Wyvern and Automaton retain independent settings; Pet Target opens the shared target settings. Visibility is controlled by Enabled and Show this pet type. `xiui_config_input_smoke --pet-tabs-only` checks exclusive selection through pointer and controller actions.

**General → Appearance → Config Text → Config text size (px)** adjusts configuration text live from 8–18 pixels, next to Config font. The default is 14 pixels; headings and help text keep their relative sizes. This saves with profiles, resets with defaults and leaves HUD text sizes independent.

The header's input-mode button shows a keyboard or controller icon. Hover it to see the configured mode and Auto's current presentation; click it for the full **Auto**, **Keyboard & Mouse**, and **Controller** options. Selecting a presentation retains the existing input behavior.

Click the **search icon** in the header to animate it into the **Search settings...** input, or press **Ctrl+F** to open and focus it directly. It uses the shared softly rounded, inset input surface and native control palette without an outline stroke. A subtle underline and caret indicate typing focus. The shortcut appears as a smaller hint at the right when empty; the header collapses back to the icon when results close, retaining the query for next time. An **X** appears once you type; click it to clear the query and keep typing focus. Search combines words across setting names, their category/topic/element path and help text. For example, **party hp**, **party hp height**, or **party hp font** finds the corresponding Party controls, including HP-specific bar and text elements.

Results use a darker native-themed panel for readability. Click a result or use Up/Down and Enter to open it. Configuration selects its category, view, topic and bar/text element, scrolls to the control and briefly highlights it. Search does not apply presets, alter values or enable disabled modules. Results follow the current configuration's conditional controls; changing a mode and reopening search refreshes the catalog.

Mouse wheel and Page Up/Down browse longer result lists. Escape closes search; Ctrl+F selects the current query for replacement. Controller **Utilities → Search** opens the same field: use a keyboard to type, directions to select/page results, Confirm to open, Actions to clear and Cancel to close. Foreground loss closes search and releases typing ownership, preserving ordinary chat drafts.

Focused checks: `xiui_config_input_smoke --settings-search-only` verifies matching, index isolation, HP bar/text navigation, disabled-category inspection, input release, icon expansion/collapse, clear-button focus retention and shared mouse/controller actions. The 6,541-entry default catalog built in about 19 ms in the focused check; live appearance remains for Cody to review.

Use the arrow at the top-left to collapse the category sidebar to icons. Hover an icon to see its category name; click it as usual. Click the arrow again to restore labels. The saved choice reloads with the configuration layout, and the settings area immediately uses the freed width.

Profile selection, creation and actions remain available as three icons at the bottom of the compact sidebar. Their tooltips identify each action and the current profile; menus retain their normal readable width. Minimum window height reserves room for both navigation and profile controls.

Controller **Utilities → Collapse sidebar to icons / Expand sidebar** uses the same action. Category names remain available through focus help, and reflow preserves the selected page and focused setting. Focused verification: `xiui_config_input_smoke --config-sidebar-only` and `--config-resize-only`.

Drag an edge or corner to resize Configuration. The lower-right grip uses the current theme. The chosen position and dimensions save with the normal configuration layout.

Settings reflow between one and two columns according to the available content width, including space taken by topic navigation and the scrollbar. Font sizes remain unchanged. Related width/height and X/Y controls stay adjacent in the settings order. Preset cards, element selectors and custom list editors use the available content area too.

Rendering, pointer controls, scrolling and controller navigation use the same layout and option slots. Reflow retains the current page and keeps the focused setting visible. Minimum dimensions preserve usable controls and readable navigation; the window stays inside the game viewport.

Keyboard users can start resizing with **Alt+F8**, adjust with the arrow keys, press **Space** for one-pixel steps, and use **Enter** to save or **Escape** to cancel. Controller users can select **Resize** in Configuration's Utilities region and use directional input, Actions for fine steps, Confirm and Cancel. Both routes use the same size constraints as dragging. Keyboard resizing also works when controller enumeration is disabled.

Resizing changes only the configuration window's layout. It does not apply presets or change HUD preferences.
