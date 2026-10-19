# Party job text

Open **Party → Text** and select **Jobs** (the same path exists for either Alliance list). All job text controls live here: Show job text, main/subjob/Master Level switches, stacking, anchor, name placement, font, size, alignment, X/Y offsets, colors and effects.

- **Stack job and subjob:** puts the main job above the subjob. For example, `WHM99/RDM49 ML20` becomes `WHM99 ML20` above `RDM49`. Existing main/subjob level and Master Level switches still apply. Hiding Subjob removes the second line.
- **Anchor:** choose **Name** to retain After name / Below name placement, or **HP bar start** to follow the actual HP fill origin, including bar position and X/Y offsets. Name placement is disabled while anchored to HP.

The font, size, color, effects, Alignment and X/Y offsets below apply to both lines and either anchor. Name movement does not move HP-anchored text. Stacked text reserves its visible height, and remote members retain their separate Zone text layout.

**Party → Job Icons** contains icon visibility, size, anchoring and X/Y offsets only. Text controls are not duplicated there. Icons do not reserve horizontal entry space, shrink the bar canvas or widen the fitted window; their anchors and offsets control drawing independently. The shared text Alignment control describes Left/Center/Right within the chosen anchor; Name placement chooses After name / Below name.

Existing configurations retain inline, name-anchored text until changed. Party/Alliance settings save `stack_jobs` and `job_anchor` independently, including profiles, resets, preference copies and presets. New controls reuse the shared mouse, keyboard and controller configuration paths.

Focused verification: `xiui_config_input_smoke --party-entry-only` covers formatting, stacked height, hidden subjobs, HP anchoring independent of the name, settings ownership and persistence, plus the existing party entry checks.
