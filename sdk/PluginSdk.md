# Yi3D Plugin SDK: Lifecycle and Unload Contract

This document is the detailed contract referenced by `inc/plugin/Plugin.h`.
Keep the header short; put behavioral notes here.

## Plugin lifecycle

### `initialize()`

Called once after the plugin module is loaded. Register commands, UI,
factories, reactors, timers, threads, and any other plugin resources here.

### `terminate()`

Called during application shutdown or before the host unloads the plugin DLL.
Release everything the plugin owns:

- every host registration the plugin made (commands, gizmo/element-node
  factories, snap rules, reactors, dock widgets, timers, threads, ...);
- every plugin-owned QObject (delete it here);
- a plugin-created `QDockWidget` must be removed from its `QMainWindow` with
  `removeDockWidget()` before it is deleted. Deleting it while it is still
  docked leaves a stale pointer in the main-window layout and can crash later
  UI-state saving;
- strings that can outlive the plugin module must be heap-owned as well.
  `QMainWindow::removeDockWidget()` keeps a shared copy of the dock's
  `objectName()` in a `QPlaceHolderItem`, and `saveState()` reads it later. If
  `objectName`/`windowTitle`/action text was assigned from `QStringLiteral`, the
  shared `QStringData` lives in the plugin module and becomes invalid after
  `FreeLibrary`/`dlclose`. Use `QString::fromLatin1`/`fromUtf8` (or deep-copy the
  strings before removal) for any value that may survive the module.

After `terminate()` returns, the host destroys the `Plugin` object and unloads
the module.

## Unload support (`supportsUnload`)

The default is `false`: the plugin is locked (ObjectARX-style). Updating or
removing it requires restarting the application. A locked plugin does not need
to remove its host-owned UI: the environment UI owns that UI and destroys it on
environment exit/suspend.

Override and return `true` only when all of the following hold:

- `terminate()` releases every host registration and plugin-owned QObject;
- `terminate()` removes, item by item, every menu/tool bar/action group/command
  action the plugin added to a `GatewayEnvironment` or `ModelingEnvironment`
  (Document) UI, using `EnvironmentUI::deleteMenu` / `deleteToolBar` /
  `deleteActionGroup` / `deleteCommandAction`;
- the plugin keeps separate per-environment pointers (QPointer recommended) if
  it adds UI to both Gateway and Modeling.

Host restrictions:

- The host refuses to unload while any Task environment (Sketch/Sketch 3D) is
  active. A plugin never has to clean up Task-environment UI on unload; the
  host destroys that UI on suspend/exit.
- The host refuses to unload while a plugin-created environment is active.
- Runtime plugin unload calls `terminate()` only; it does **not** replay
  `onEnvironmentToBeExited`. Environment exit and application shutdown still
  deliver `onEnvironmentToBeExited`.

Object removal is per `EnvironmentUI` instance: `deleteMenu` / `deleteToolBar` /
`deleteActionGroup` / `deleteCommandAction` only touch objects present in that
UI's bookkeeping; passing an object created by another environment UI is
ignored.

All `EnvironmentUI` creation methods (`newMenu`, `newToolBar`, `newActionGroup`,
`newCommandAction`) return objects owned by that UI. A plugin must not delete
them directly; remove them with the matching `deleteMenu`, `deleteToolBar`,
`deleteActionGroup`, or `deleteCommandAction`. Removal uses `deleteLater`, so
the pointer is invalid after the call.

`EnvironmentUI` itself is implemented by the host; plugins only hold a borrowed
pointer. `getDefaultCommandName` / `setDefaultCommandName` query or customize
the command posted when the environment is entered/resumed and restarted after
a modal command ends. The default is `"Select"`; a custom command must be
registered by that environment while it is active.

### wyrx ClassInfo on reload

The wyrx `ClassInfo` registry is process-wide and keeps the old `ObjectFactory`
pointer into the unloaded module. A plugin that registers wyrx classes (for
example command classes declared with `WYRX_DECLARE_MEMBERS`) must not let a
reload reuse a `ClassInfo` registered by the previous load:

- use stable class names and remove the stale entries with
  `wyrx::deleteClassInfo()` before registering classes on the next load;
- doing it at the start of `initialize()` is safer than in `terminate()`, where
  command executions may still hold `ClassInfo` pointers;
- if that cannot be done, use per-load unique class names or keep
  `supportsUnload()` returning `false`.

## Environment callbacks

- `onEnvironmentEntered`: create the plugin's per-environment UI and
  registrations.
- `onEnvironmentToBeExited`: the environment is leaving; remove the plugin's
  per-environment objects. During normal environment exit the host also
  destroys host-owned UI, but on runtime plugin unload this callback is not
  replayed; `terminate()` must do the cleanup.
- `onEnvironmentToBeSuspended`: the host destroys the environment UI; release
  pointers (QPointer recommended) or be prepared to re-create them on resume.
- `onEnvironmentResumed`: re-create UI as needed.

## Command execution from plugin code

The host blocks its command entry points (`CommandAction` and IPC
`command`/`script`) while the command stack is transitional, for example during
plugin load/unload or while the Plugin Manager dialog is open. Plugin code must
not bypass those entry points by calling `CmdManager::executeCommand` /
`postCommand` directly from timers, `initialize()`, `terminate()`, or
environment callbacks. Use a host-provided `CommandAction` (or the host API
that owns the operation) instead.

## Checklist for `supportsUnload() == true`

- [ ] `terminate()` removes every menu/tool bar/action group/command action
      added to Gateway/Modeling.
- [ ] `terminate()` deletes all plugin-owned QObjects; dock widgets are removed
      before deletion.
- [ ] strings that can outlive the module are not `QStringLiteral`.
- [ ] wyrx `ClassInfo` entries are handled (stable names + `deleteClassInfo`,
      or per-load unique names).
- [ ] no reliance on `onEnvironmentToBeExited` during runtime unload.
- [ ] per-environment pointers are kept separately (QPointer recommended).
