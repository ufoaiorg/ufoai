-- MojoSetup; a portable, flexible installation application.
--
-- Please see the file LICENSE.txt in the source's root directory.
--
--  This file written by Ryan C. Gordon.


-- This is where most of the magic happens. Everything is initialized, and
--  the user's config script has successfully run. This Lua chunk drives the
--  main application code.

-- !!! FIXME: add a --dryrun option.

-- This is just for convenience.
local _ = MojoSetup.translate

local function do_delete(fname)
    local retval = false
    if fname == nil then
        retval = true
    else
        if MojoSetup.platform.exists(fname) then
            if MojoSetup.platform.unlink(fname) then
                MojoSetup.loginfo("Deleted '" .. fname .. "'")
                retval = true
            end
        end
    end
    return retval
end

local function delete_files(filelist, callback, error_is_fatal)
    if filelist ~= nil then
        local max = #filelist
        for i = max,1,-1 do
            local fname = filelist[i]
            if not do_delete(fname) and error_is_fatal then
                -- !!! FIXME: formatting
                MojoSetup.fatal(_("Deletion failed!"))
            end

            if callback ~= nil then
                callback(i, max)
            end
        end
    end
end

local function delete_rollbacks()
    if MojoSetup.rollbacks == nil then
        return
    end
    local fnames = {}
    local max = #MojoSetup.rollbacks
    for id = 1,max,1 do
        fnames[id] = MojoSetup.rollbackdir .. "/" .. id
    end
    MojoSetup.rollbacks = {}   -- just in case this gets called again...
    delete_files(fnames)
end

local function delete_scratchdirs()
    do_delete(MojoSetup.downloaddir)
    do_delete(MojoSetup.rollbackdir)
    do_delete(MojoSetup.scratchdir)  -- must be last!
end


local function do_rollbacks()
    if MojoSetup.rollbacks == nil then
        return
    end

    local max = #MojoSetup.rollbacks
    for id = max,1,-1 do
        local src = MojoSetup.rollbackdir .. "/" .. id
        local dest = MojoSetup.rollbacks[id]
        if not MojoSetup.movefile(src, dest) then
            -- we're already in fatal(), so we can only throw up a msgbox...
            -- !!! FIXME: formatting
            MojoSetup.msgbox(_("Serious problem"),
                             _("Couldn't restore some files. Your existing installation is likely damaged."))
        end
        MojoSetup.loginfo("Restored rollback #" .. id .. ": '" .. src .. "' -> '" .. dest .. "'")
    end

    MojoSetup.rollbacks = {}   -- just in case this gets called again...
end


-- This gets called by fatal()...must be a global function.
function MojoSetup.revertinstall()
    -- !!! FIXME: language.
    if MojoSetup.gui_started then
        MojoSetup.gui.final(_("There were errors. We will now put things back how we found them and exit."));
    end

    MojoSetup.loginfo("Cleaning up half-finished installation...");

    -- !!! FIXME: callbacks here.
    delete_files(MojoSetup.downloads)
    delete_files(MojoSetup.installed_files)
    do_rollbacks()
    delete_scratchdirs();
end


local function calc_percent(current, total)
    if total == 0 then
        return 0
    elseif total < 0 then
        return -1
    end

    local retval = MojoSetup.truncatenum((current / total) * 100)
    if retval > 100 then
        retval = 100
    elseif retval < 0 then
        retval = 0
    end
    return retval
end


local function make_bps_string(bps, bw, total)
    -- !!! FIXME: localization on all this.
    local bpsstr = nil

    if bps <= 0 then
        bpsstr = _("(stalled)")
    else
        local bytesleft = total - bw
        local secsleft = MojoSetup.truncatenum(bytesleft / bps)
        local minsleft = MojoSetup.truncatenum(secsleft / 60)
        local hoursleft = MojoSetup.truncatenum(minsleft / 60)

        secsleft = string.sub("00" .. (secsleft - (minsleft * 60)), -2)
        minsleft = string.sub("00" .. (minsleft - (hoursleft * 60)), -2)

        if hoursleft < 10 then
            hoursleft = "0" .. hoursleft;
        else
            hoursleft = tostring(hoursleft)
        end

        local timeleft = hoursleft .. ":" .. minsleft .. ":" .. secsleft
        if bps > 1024 then
            local kps = MojoSetup.truncatenum(bps / 1024)
            if total > 0 then
                bpsstr = " (" .. kps .. _("Kb/s") .. ", " .. timeleft .. " remaining)"
            else
                bpsstr = " (" .. kps .. _("Kb/s") .. ")"
            end
        else
            if total > 0 then
                bpsstr = " (" .. bps .. _("b/s") .. ", " .. timeleft .. " remaining)"
            else
                bpsstr = " (" .. bps .. _("b/s") .. ")"
            end
        end
    end
    return bpsstr
end


local function split_path(path)
    local retval = {}
    for item in string.gmatch(path .. "/", "(.-)/") do
        if item ~= "" then
            retval[#retval+1] = item
        end
    end
    return retval
end

local function rebuild_path(paths, n)
    local retval = paths[n]
    n = n + 1
    while paths[n] ~= nil do
        retval = retval .. "/" .. paths[n]
        n = n + 1
    end
    return retval
end

local function normalize_path(path)
    return rebuild_path(split_path(path), 1)
end


local function close_archive_list(arclist)
    for i = #arclist,1,-1 do
        MojoSetup.archive.close(arclist[i])
        arclist[i] = nil
    end
end


-- This code's a little nasty...
local function drill_for_archive(archive, path, arclist)
    if not MojoSetup.archive.enumerate(archive) then
        MojoSetup.fatal(_("Couldn't enumerate archive."))  -- !!! FIXME: error message
    end

    local pathtab = split_path(path)
    local ent = MojoSetup.archive.enumnext(archive)
    while ent ~= nil do
        if ent.type == "file" then
            local i = 1
            local enttab = split_path(ent.filename)
            while (enttab[i] ~= nil) and (enttab[i] == pathtab[i]) do
                i = i + 1
            end

            if enttab[i] == nil then
                -- It's a file that makes up some of the specified path.
                --  open it as an archive and keep drilling...
                local arc = MojoSetup.archive.fromentry(archive)
                if arc == nil then
                    MojoSetup.fatal(_("Couldn't open archive."))  -- !!! FIXME: error message.
                end
                arclist[#arclist+1] = arc
                if pathtab[i] == nil then
                    return arc  -- this is the end of the path! Done drilling!
                end
                return drill_for_archive(arc, rebuild_path(pathtab, i), arclist)
            end
        end
        ent = MojoSetup.archive.enumnext(archive)
    end

    MojoSetup.fatal(_("Archive not found."))  -- !!! FIXME: error message.
end


local function install_file(dest, archive, file, perms, option)
    -- Upvalued so we don't look these up each time...
    local fname = string.gsub(dest, "^.*/", "", 1)  -- chop the dirs off...
    local ptype = _("Installing")  -- !!! FIXME: localization.
    local component = option.description
    local keepgoing = true
    local callback = function(ticks, justwrote, bw, total)
        local percent = -1
        local item = fname
        if total >= 0 then
            MojoSetup.written = MojoSetup.written + justwrote
            percent = calc_percent(MojoSetup.written, MojoSetup.totalwrite)
            item = fname .. ": " .. calc_percent(bw, total) .. "%"  -- !!! FIXME: localization
        end
        keepgoing = MojoSetup.gui.progress(ptype, component, percent, item)
        return keepgoing
    end

    MojoSetup.installed_files[#MojoSetup.installed_files+1] = dest
    local written, sums = MojoSetup.writefile(archive, dest, perms, callback)
    if not written then
        -- !!! FIXME: formatting!
        if not keepgoing then
            MojoSetup.logerror("User cancelled install during file write.")
            MojoSetup.fatal(_("User cancelled installation."))
        else
            MojoSetup.logerror("Failed to create file '" .. dest .. "'")
            MojoSetup.fatal(_("File creation failed!"))
        end
    end

    -- !!! FIXME: perms may be null...we need a MojoSetup.defaultPermsString()...

    if option ~= nil then
        if MojoSetup.manifest[option] == nil then
            MojoSetup.manifest[option] = {}
        end
        local manifest = MojoSetup.manifest[option]
        manifest[#manifest+1] =
        {
            type = "file",
            path = dest,
            checksums = sums,
            mode = perms,
        }
    end

    MojoSetup.loginfo("Created file '" .. dest .. "'")
end


local function install_symlink(dest, lndest, option)
    MojoSetup.installed_files[#MojoSetup.installed_files+1] = dest
    if not MojoSetup.platform.symlink(dest, lndest) then
        -- !!! FIXME: formatting!
        MojoSetup.logerror("Failed to create symlink '" .. dest .. "'")
        MojoSetup.fatal(_("symlink creation failed!"))
    end

    if option ~= nil then
        if MojoSetup.manifest[option] == nil then
            MojoSetup.manifest[option] = {}
        end
        local manifest = MojoSetup.manifest[option]
        manifest[#manifest+1] =
        {
            type = "symlink",
            path = dest,
            linkdest = lndest,
        }
    end

    MojoSetup.loginfo("Created symlink '" .. dest .. "' -> '" .. lndest .. "'")
end


local function install_directory(dest, perms, option)
    MojoSetup.installed_files[#MojoSetup.installed_files+1] = dest
    if not MojoSetup.platform.mkdir(dest, perms) then
        -- !!! FIXME: formatting
        MojoSetup.logerror("Failed to create dir '" .. dest .. "'")
        MojoSetup.fatal(_("mkdir failed"))
    end

    if option ~= nil then
        if MojoSetup.manifest[option] == nil then
            MojoSetup.manifest[option] = {}
        end
        local manifest = MojoSetup.manifest[option]
        manifest[#manifest+1] =
        {
            type = "dir",
            path = dest,
            mode = perms,
        }
    end

    MojoSetup.loginfo("Created directory '" .. dest .. "'")
end


local function install_parent_dirs(path, option)
    -- Chop any '/' chars from the end of the string...
    path = string.gsub(path, "/+$", "")

    -- Build each piece of final path. The gmatch() skips the last element.
    local fullpath = ""
    for item in string.gmatch(path, "(.-)/") do
        if item ~= "" then
            fullpath = fullpath .. "/" .. item
            if not MojoSetup.platform.exists(fullpath) then
                install_directory(fullpath, nil, option)
            end
        end
    end
end


local function permit_write(dest, entinfo, file)
    local allowoverwrite = true
    if MojoSetup.platform.exists(dest) then
        -- never "permit" existing dirs, so they don't rollback.
        if entinfo.type == "dir" then
            allowoverwrite = false
        else
            if MojoSetup.forceoverwrite ~= nil then
                allowoverwrite = MojoSetup.forceoverwrite
            else
                -- !!! FIXME: option/package-wide overwrite?
                allowoverwrite = file.allowoverwrite
                if not allowoverwrite then
                    -- !!! FIXME: language and formatting.
                    MojoSetup.loginfo("File '" .. dest .. "' already exists.")
                    local ynan = MojoSetup.promptynan(_("Conflict!"), _("File already exists! Replace?"), true)
                    if ynan == "always" then
                        MojoSetup.forceoverwrite = true
                        allowoverwrite = true
                    elseif ynan == "never" then
                        MojoSetup.forceoverwrite = false
                        allowoverwrite = false
                    elseif ynan == "yes" then
                        allowoverwrite = true
                    elseif ynan == "no" then
                        allowoverwrite = false
                    end
                end
            end

            -- !!! FIXME: Setup.File.mustoverwrite to override "never"?

            if allowoverwrite then
                local id = #MojoSetup.rollbacks + 1
                local f = MojoSetup.rollbackdir .. "/" .. id
                -- !!! FIXME: don't add (f) to the installed_files table...
                install_parent_dirs(f, nil)
                MojoSetup.rollbacks[id] = dest
                if not MojoSetup.movefile(dest, f) then
                    -- !!! FIXME: formatting
                    MojoSetup.fatal(_("Couldn't backup file for rollback"))
                end
                MojoSetup.loginfo("Moved rollback #" .. id .. ": '" .. dest .. "' -> '" .. f .. "'")
            end
        end
    end

    return allowoverwrite
end


local function install_archive_entry(archive, ent, file, option)
    local entdest = ent.filename
    if entdest == nil then return end   -- probably can't happen...

    -- Set destination in native filesystem. May be default or explicit.
    local dest = file.destination
    if dest == nil then
        dest = entdest
    else
        dest = dest .. "/" .. entdest
    end

    local perms = file.permissions   -- may be nil

    if file.filter ~= nil then
        local filterperms
        dest, filterperms = file.filter(dest)
        if filterperms ~= nil then
            perms = filterperms
        end
    end

    if dest ~= nil then  -- Only install if file wasn't filtered out
        dest = MojoSetup.destination .. "/" .. dest
        if permit_write(dest, ent, file) then
            install_parent_dirs(dest, option)
            if ent.type == "file" then
                install_file(dest, archive, file, perms, option)
            elseif ent.type == "dir" then
                install_directory(dest, perms, option)
            elseif ent.type == "symlink" then
                install_symlink(dest, ent.linkdest, option)
            else  -- !!! FIXME: device nodes, etc...
                -- !!! FIXME: formatting!
                -- !!! FIXME: should this be fatal?
                MojoSetup.fatal(_("Unknown file type in archive"))
            end
        end
    end
end


local function install_archive(archive, file, option)
    if not MojoSetup.archive.enumerate(archive) then
        -- !!! FIXME: need formatting function.
        MojoSetup.fatal(_("Can't enumerate archive"))
    end

    local isbase = (archive == MojoSetup.archive.base)
    local single_match = true
    local wildcards = file.wildcards

    -- If there's only one explicit file we're looking for, we don't have to
    --  iterate the whole archive...we can stop as soon as we find it.
    if wildcards == nil then
        single_match = false
    else
        if type(wildcards) == "string" then
            wildcards = { wildcards }
        end
        if #wildcards > 1 then
            single_match = false
        else
            for k,v in ipairs(wildcards) do
                if string.find(v, "[*?]") ~= nil then
                    single_match = false
                    break  -- no reason to keep iterating...
                end
            end
        end
    end

    local ent = MojoSetup.archive.enumnext(archive)
    while ent ~= nil do
        -- If inside GBaseArchive (no URL lead in string), then we
        --  want to clip to data/ directory...
        if isbase then
            local count
            ent.filename, count = string.gsub(ent.filename, "^data/", "", 1)
            if count == 0 then
                ent.filename = nil
            end
        end

        -- See if we should install this file...
        if (ent.filename ~= nil) and (ent.filename ~= "") then
            local should_install = false
            if wildcards == nil then
                should_install = true
            else
                for k,v in ipairs(wildcards) do
                    if MojoSetup.wildcardmatch(ent.filename, v) then
                        should_install = true
                        break  -- no reason to keep iterating...
                    end
                end
            end

            if should_install then
                install_archive_entry(archive, ent, file, option)
                if single_match then
                    break   -- no sense in iterating further if we're done.
                end
            end
        end

        -- and check the next entry in the archive...
        ent = MojoSetup.archive.enumnext(archive)
    end
end


local function install_basepath(basepath, file, option)
    -- Obviously, we don't want to enumerate the entire physical filesystem,
    --  so we'll dig through each path element with MojoPlatform_exists()
    --  until we find one that doesn't, then we'll back up and try to open
    --  that as a directory, and then a file archive, and start drilling from
    --  there. Fun.

    local function create_basepath_archive(path)
        local archive = MojoSetup.archive.fromdir(path)
        if archive == nil then
            archive = MojoSetup.archive.fromfile(path)
            if archive == nil then  -- !!! FIXME: error message.
                MojoSetup.fatal(_("cannot open archive."))
            end
        end
        return archive
    end

    -- fast path: See if the whole path exists. This is probably the normal
    --  case, but it won't work for archives-in-archives.
    if MojoSetup.platform.exists(basepath) then
        local archive = create_basepath_archive(basepath)
        install_archive(archive, file, option)
        MojoSetup.archive.close(archive)
    else
        -- Check for archives-in-archives...
        local path = ""
        local paths = split_path(basepath)
        for i,v in ipairs(paths) do
            local knowngood = path
            path = path .. "/" .. v
            if not MojoSetup.platform.exists(path) then
                if knowngood == "" then  -- !!! FIXME: error message.
                    MojoSetup.fatal(_("archive not found"))
                end
                local archive = create_basepath_archive(knowngood)
                local arclist = { archive }
                path = rebuild_path(paths, i)
                local arc = drill_for_archive(archive, path, arclist)
                install_archive(arc, file, option)
                close_archive_list(arclist)
                return  -- we're done here
            end
        end

        -- wait, the whole thing exists now? Did this just move in?
        install_basepath(basepath, file, option)  -- try again, I guess...
    end
end


local function set_destination(dest)
    -- !!! FIXME: ".mojosetup_tmp" dirname may clash with install...?
    MojoSetup.loginfo("Install dest: '" .. dest .. "'")
    MojoSetup.destination = dest
    MojoSetup.scratchdir = MojoSetup.destination .. "/.mojosetup_tmp"
    MojoSetup.rollbackdir = MojoSetup.scratchdir .. "/rollbacks"
    MojoSetup.downloaddir = MojoSetup.scratchdir .. "/downloads"
end


local function run_config_defined_hook(func, install)
    if func ~= nil then
        local errstr = func(install)
        if errstr ~= nil then
            MojoSetup.fatal(errstr)
        end
    end
end


local function do_install(install)
    MojoSetup.forceoverwrite = nil
    MojoSetup.written = 0
    MojoSetup.totalwrite = 0
    MojoSetup.downloaded = 0
    MojoSetup.totaldownload = 0
    MojoSetup.install = install

    -- !!! FIXME: try to sanity check everything we can here
    -- !!! FIXME:  (unsupported URLs, bogus media IDs, etc.)
    -- !!! FIXME:  I would like everything possible to fail here instead of
    -- !!! FIXME:  when a user happens to pick an option no one tested...

    if install.options ~= nil and install.optiongroups ~= nil then
        MojoSetup.fatal(_("Config bug: no options"))
    end

    -- This is to save us the trouble of a loop every time we have to
    --  find media by id...
    MojoSetup.media = {}
    if install.medias ~= nil then
        for k,v in pairs(install.medias) do
            if MojoSetup.media[v.id] ~= nil then
                MojoSetup.fatal(_("Config bug: duplicate media id"))
            end
            MojoSetup.media[v.id] = v
        end
    end

    -- Build a bunch of functions into a linear array...this lets us move
    --  back and forth between stages of the install with customized functions
    --  for each bit that have their own unique params embedded as upvalues.
    -- So if there are three EULAs to accept, we'll call three variations of
    --  the EULA function with three different tables that appear as local
    --  variables, and the driver that calls this function will be able to
    --  skip back and forth based on user input. This is a cool Lua thing.
    local stages = {}

    -- First stage: Make sure installer can run. Always fails or steps forward.
    if install.precheck ~= nil then
        stages[#stages+1] = function ()
            run_config_defined_hook(install.precheck, install)
            return 1
        end
    end

    -- Next stage: accept all global EULAs. Rejection of any EULA is considered
    --  fatal. These are global EULAs for the install, per-option EULAs come
    --  later.
    if install.eulas ~= nil then
        for k,eula in pairs(install.eulas) do
            local desc = eula.description
            local fname = "data/" .. eula.source

            -- (desc) and (fname) become an upvalues in this function.
            stages[#stages+1] = function (thisstage, maxstage)
                local retval = MojoSetup.gui.readme(desc,fname,thisstage,maxstage)
                if retval == 1 then
                    if not MojoSetup.promptyn(desc, _("Accept this license?"), false) then
                        MojoSetup.fatal(_("You must accept the license before you may install"))
                    end
                end
                return retval
            end
        end
    end

    -- Next stage: show any READMEs.
    if install.readmes ~= nil then
        for k,readme in pairs(install.readmes) do
            local desc = readme.description
            -- !!! FIXME: pull from archive?
            local fname = "data/" .. readme.source
            -- (desc) and (fname) become upvalues in this function.
            stages[#stages+1] = function(thisstage, maxstage)
                return MojoSetup.gui.readme(desc, fname, thisstage, maxstage)
            end
        end
    end

    -- Next stage: let user choose install destination.
    --  The config file can force a destination if it has a really good reason
    --  (system drivers that have to go in a specific place, for example),
    --  but really really shouldn't in 99% of the cases.
    if install.destination ~= nil then
        set_destination(install.destination)
    else
        local recommend = nil
        local recommended_cfg = install.recommended_destinations
        if recommended_cfg ~= nil then
            if type(recommended_cfg) == "string" then
                recommended_cfg = { recommended_cfg }
            end

            recommend = {}
            for i,v in ipairs(recommended_cfg) do
                if MojoSetup.platform.isdir(v) then
                    if MojoSetup.platform.writable(v) then
                        recommend[#recommend+1] = v .. "/" .. install.id
                    end
                end
            end

            if #recommend == 0 then
                recommend = nil
            end
        end

        -- (recommend) becomes an upvalue in this function.
        stages[#stages+1] = function(thisstage, maxstage)
            local rc, dst
            rc, dst = MojoSetup.gui.destination(recommend, thisstage, maxstage)
            if rc == 1 then
                set_destination(dst)
            end
            return rc
        end
    end

    -- Next stage: let user choose install options.
    --  This may not produce a GUI stage if it decides that all options
    --  are either required or disabled.
    -- (install) becomes an upvalue in this function.
    stages[#stages+1] = function(thisstage, maxstage)
        -- This does some complex stuff with a hierarchy of tables in C.
        return MojoSetup.gui.options(install, thisstage, maxstage)
    end


    -- Next stage: accept all option-specific EULAs.
    --  Rejection of any EULA will put you back one stage (usually to the
    --  install options), but if there is no previous stage, this becomes
    --  fatal.
    -- This may not produce a GUI stage if there are no selected options with
    --  EULAs to accept.
    stages[#stages+1] = function(thisstage, maxstage)
        local option_eulas = {}

        local function find_option_eulas(opts)
            local options = opts['options']
            if options ~= nil then
                for k,v in pairs(options) do
                    if v.value and v.eulas then
                        for ek,ev in pairs(v.eulas) do
                            option_eulas[#option_eulas+1] = ev
                        end
                        find_option_eulas(v)
                    end
                end
            end

            local optiongroups = opts['optiongroups']
            if optiongroups ~= nil then
                for k,v in pairs(optiongroups) do
                    if not v.disabled then
                        find_option_eulas(v)
                    end
                end
            end
        end

        find_option_eulas(install)

        for k,eula in pairs(option_eulas) do
            local desc = eula.description
            local fname = "data/" .. eula.source
            local retval = MojoSetup.gui.readme(desc,fname,thisstage,maxstage)
            if retval == 1 then
                if not MojoSetup.promptyn(desc, _("Accept this license?"), false) then
                    -- can't go back? We have to die here instead.
                    if thisstage == 1 then
                        MojoSetup.fatal(_("You must accept the license before you may install"))
                    else
                        retval = -1  -- just step back a stage.
                    end
                end
            end

            if retval ~= 1 then
                return retval
            end
        end

        return 1   -- all licenses were agreed to. Go to the next stage.
    end

    -- Next stage: Make sure source list is sane.
    --  This is not a GUI stage, it just needs to run between them.
    --  This gets a little hairy.
    stages[#stages+1] = function(thisstage, maxstage)
        local function process_file(option, file)
            -- !!! FIXME: what happens if a file shows up in multiple options?
            local src = file.source
            local prot,host,path
            if src ~= nil then  -- no source, it's in GBaseArchive
                prot,host,path = MojoSetup.spliturl(src)
            end
            if (src == nil) or (prot == "base://") then  -- included content?
                if MojoSetup.files.included == nil then
                    MojoSetup.files.included = {}
                end
                MojoSetup.files.included[file] = option
            elseif prot == "media://" then
                -- !!! FIXME: make sure media id is valid.
                if MojoSetup.files.media == nil then
                    MojoSetup.files.media = {}
                end
                if MojoSetup.files.media[host] == nil then
                    MojoSetup.files.media[host] = {}
                end
                MojoSetup.files.media[host][file] = option
            else
                -- !!! FIXME: make sure we can handle this URL...
                if MojoSetup.files.downloads == nil then
                    MojoSetup.files.downloads = {}
                end

                if option.bytes > 0 then
                    MojoSetup.totaldownload = MojoSetup.totaldownload + option.bytes
                end
                MojoSetup.files.downloads[file] = option
            end
        end

        -- Sort an option into tables that say what sort of install it is.
        --  This lets us batch all the things from one CD together,
        --  do all the downloads first, etc.
        local function process_option(option)
            if option.bytes > 0 then
                MojoSetup.totalwrite = MojoSetup.totalwrite + option.bytes
            end
            if option.files ~= nil then
                for k,v in pairs(option.files) do
                    process_file(option, v)
                end
            end
        end

        local function build_source_tables(opts)
            local options = opts['options']
            if options ~= nil then
                for k,v in pairs(options) do
                    if v.value then
                        process_option(v)
                        build_source_tables(v)
                    end
                end
            end

            local optiongroups = opts['optiongroups']
            if optiongroups ~= nil then
                for k,v in pairs(optiongroups) do
                    if not v.disabled then
                        build_source_tables(v)
                    end
                end
            end
        end

        run_config_defined_hook(install.preflight, install)

        MojoSetup.files = {}
        build_source_tables(install)

        -- This dumps the files tables using MojoSetup.logdebug,
        --  so it only spits out crap if debug-level logging is enabled.
        MojoSetup.dumptable("MojoSetup.files", MojoSetup.files)

        return 1   -- always go forward from non-GUI stage.
    end

    -- Next stage: Download external packages.
    stages[#stages+1] = function(thisstage, maxstage)
        if MojoSetup.files.downloads ~= nil then
            -- !!! FIXME: id will cause problems for download resume
            -- !!! FIXME: id will chop filename extension
            local id = 0
            for file,option in pairs(MojoSetup.files.downloads) do
                local f = MojoSetup.downloaddir .. "/" .. id
                -- !!! FIXME: don't add (f) to the installed_files table...
                install_parent_dirs(f, nil)
                id = id + 1

                -- Upvalued so we don't look these up each time...
                local url = file.source
                local fname = string.gsub(url, "^.*/", "", 1)  -- chop the dirs off...
                local ptype = _("Downloading")  -- !!! FIXME: localization.
                local component = option.description
                local bps = 0
                local bpsticks = 0
                local bpsstr = ''
                local item = fname
                local percent = -1
                local callback = function(ticks, justwrote, bw, total)
                    if total < 0 then
                        -- adjust start point for d/l rate calculation...
                        bpsticks = ticks + 1000
                    else
                        if ticks >= bpsticks then
                            bpsstr = make_bps_string(bps, bw, total)
                            bpsticks = ticks + 1000
                            bps = 0
                        end
                        bps = bps + justwrote
                        MojoSetup.downloaded = MojoSetup.downloaded + justwrote
                        percent = calc_percent(MojoSetup.downloaded,
                                               MojoSetup.totaldownload)

                        item = fname .. ": " .. calc_percent(bw, total) .. "%" .. bpsstr -- !!! FIXME: localization
                    end
                    return MojoSetup.gui.progress(ptype, component, percent, item)
                end

                MojoSetup.loginfo("Download '" .. url .. "' to '" .. f .. "'")
                local downloaded, sums = MojoSetup.download(url, f, callback)
                if not downloaded then
                    -- !!! FIXME: formatting!
                    MojoSetup.fatal(_("file download failed!"))
                end
                MojoSetup.downloads[#MojoSetup.downloads+1] = f
            end
        end
        return 1
    end

    -- Next stage: actual installation.
    stages[#stages+1] = function(thisstage, maxstage)
        run_config_defined_hook(install.preinstall)

        -- Do stuff on media first, so the user finds out he's missing
        --  disc 3 of 57 as soon as possible...

        -- Since we sort all things to install by the media that contains
        --  the source data, you should only have to insert each disc
        --  once, no matter how they landed in the config file.

        if MojoSetup.files.media ~= nil then
            -- Build an array of media ids so we can sort them into a
            --  reasonable order...disc 1 before disc 2, etc.
            local medialist = {}
            for mediaid,mediavalue in pairs(MojoSetup.files.media) do
                medialist[#medialist+1] = mediaid
            end
            table.sort(medialist)

            for mediaidx,mediaid in ipairs(medialist) do
                local media = MojoSetup.media[mediaid]
                local basepath = MojoSetup.findmedia(media.uniquefile)
                while basepath == nil do
                    if not MojoSetup.gui.insertmedia(media.description) then
                        return 0   -- user cancelled.
                    end
                    basepath = MojoSetup.findmedia(media.uniquefile)
                end

                -- Media is ready, install everything from this one...
                MojoSetup.loginfo("Found correct media at '" .. basepath .. "'")
                local files = MojoSetup.files.media[mediaid]
                for file,option in pairs(files) do
                    local prot,host,path = MojoSetup.spliturl(file.source)
                    install_basepath(basepath .. "/" .. path, file, option)
                end
            end
            medialist = nil   -- done with this.
        end

        if MojoSetup.files.downloads ~= nil then
            local id = 0
            for file,option in pairs(MojoSetup.files.downloads) do
                local f = MojoSetup.downloaddir .. "/" .. id
                id = id + 1
                install_basepath(f, file, option)
            end
        end

        if MojoSetup.files.included ~= nil then
            for file,option in pairs(MojoSetup.files.included) do
                local arc = MojoSetup.archive.base
                if file.source == nil then
                    install_archive(arc, file, option)
                else
                    local prot,host,path = MojoSetup.spliturl(file.source)
                    local arclist = {}
                    arc = drill_for_archive(arc, "data/" .. path, arclist)
                    install_archive(arc, file, option)
                    close_archive_list(arclist)
                end
            end
        end

        run_config_defined_hook(install.postinstall, install)
        return 1   -- go to next stage.
    end

    -- Next stage: show results gui
    stages[#stages+1] = function(thisstage, maxstage)
        -- !!! FIXME: language.
        MojoSetup.gui.final(_("Installation was successful."))
        return 1  -- go forward.
    end


    -- Now make all this happen.

    local splashfname = install.splash
    if splashfname ~= nil then
        splashfname = 'meta/' .. splashfname
    end

    if not MojoSetup.gui.start(install.description, splashfname) then
        MojoSetup.fatal(_("GUI failed to start"))
    end

    MojoSetup.gui_started = true

    -- Make the stages available elsewhere.
    MojoSetup.stages = stages

    MojoSetup.manifest = {}
    MojoSetup.installed_files = {}
    MojoSetup.rollbacks = {}
    MojoSetup.downloads = {}

    local i = 1
    while MojoSetup.stages[i] ~= nil do
        local stage = MojoSetup.stages[i]
        local rc = stage(i, #MojoSetup.stages)

        -- Too many times I forgot to return something.   :)
        if type(rc) ~= "number" then
            MojoSetup.fatal(_("BUG: stage returned wrong type."))
        end

        if rc == 1 then
            i = i + 1   -- next stage.
        elseif rc == -1 then
            if i == 1 then
                MojoSetup.fatal(_("BUG: stepped back over start of stages"))
            else
                i = i - 1
            end
        elseif rc == 0 then
            MojoSetup.fatal(_("User cancelled installation."))
        else
            MojoSetup.fatal(_("BUG: stage returned wrong value."))
        end
    end

    -- !!! FIXME: write out manifest.

    -- Successful install, so delete conflicts we no longer need to rollback.
    delete_rollbacks()
    delete_files(MojoSetup.downloads)
    delete_scratchdirs()

    -- Don't let future errors delete files from successful installs...
    MojoSetup.installed_files = nil
    MojoSetup.downloads = nil
    MojoSetup.rollbacks = nil

    MojoSetup.gui.stop()
    MojoSetup.gui_started = nil

    -- Done with these things. Make them eligible for garbage collection.
    stages = nil
    MojoSetup.manifest = nil
    MojoSetup.destination = nil
    MojoSetup.scratchdir = nil
    MojoSetup.rollbackdir = nil
    MojoSetup.downloaddir = nil
    MojoSetup.install = nil
    MojoSetup.forceoverwrite = nil
    MojoSetup.stages = nil
    MojoSetup.files = nil
    MojoSetup.media = nil
    MojoSetup.written = 0
    MojoSetup.totalwrite = 0
    MojoSetup.downloaded = 0
    MojoSetup.totaldownload = 0
end



-- Mainline.


-- This dumps the table built from the user's config script using logdebug,
--  so it only spits out crap if debug-level logging is enabled.
MojoSetup.dumptable("MojoSetup.installs", MojoSetup.installs)

local saw_an_installer = false
for installkey,install in pairs(MojoSetup.installs) do
    if not install.disabled then
        saw_an_installer = true
        do_install(install)
        MojoSetup.collectgarbage()  -- nuke all the tables we threw around...
    end
end

if not saw_an_installer then
    MojoSetup.fatal(_("Nothing to do!"))
end
