function setChannels() {
    if (emu) {
        var mask = 0;

        if ($("#ch1").attr("checked") != "checked") mask += 0x1;
        if ($("#ch2").attr("checked") != "checked") mask += 0x2;
        if ($("#ch3").attr("checked") != "checked") mask += 0x4;
        if ($("#ch4").attr("checked") != "checked") mask += 0x8;
        if ($("#ch5").attr("checked") != "checked") mask += 0x10;
        if ($("#ch6").attr("checked") != "checked") mask += 0x20;
        if ($("#ch7").attr("checked") != "checked") mask += 0x40;
        if ($("#ch8").attr("checked") != "checked") mask += 0x80;
        //message(mask);
        Module.ccall("gme_mute_voices", "number", ["number", "number"], [emu, mask]);
    }
}

function play(fileName, trackNo) {
    play_song(fileName, trackNo);
    return false;
}

function stop() {
    node.disconnect();
    if (Module.ccall("gme_delete", "number", ["number"], [emu]) != 0)
        message("could not stop track");
}

function getFileExtension(filename) {
    return filename.substring(filename.lastIndexOf('.') + 1, filename.length);
}

function getFileName(filename) {
    return filename.substring(filename.lastIndexOf('/') + 1, filename.length);
}

function parse_metadata(ref) {
    var offset = 0;

    var read_int32 = function () {
        var value = Module.getValue(ref + offset, "i32");
        offset += 4;
        return value;
    }

    var read_string = function () {
        var value = Module.Pointer_stringify(Module.getValue(ref + offset, "i8*"));
        offset += 4;
        return value;
    }

    var res = {};

    res.length = read_int32();
    res.intro_length = read_int32();
    res.loop_length = read_int32();
    res.play_length = read_int32();

    offset += 4 * 12; // skip unused bytes

    res.system = read_string();
    res.game = read_string();
    res.song = read_string();
    res.author = read_string();
    res.copyright = read_string();
    res.comment = read_string();

    return res;
}

function updateSongInfo(filename, subtune) {
    subtune = subtune || 0;
    var subtune_count = Module.ccall("gme_track_count", "number", ["number"], [emu]);
    if (Module.ccall("gme_track_info", "number", ["number", "number", "number"], [emu, ref, subtune]) != 0)
        console.error("could not load metadata");

    var metadata = parse_metadata(Module.getValue(ref, "*"));

    tracks = buildTracksHTML(filename, subtune_count);
    $("#song_info").html(
        "FileName: " + filename.replace("nsf/", "") +
        "<br>Author: " + (metadata.author || "--") +
        "<br>Game: " + (metadata.game || "--") +
        "<br>Copyright: " + (metadata.copyright || "--") +
        "<br>Comment: " + (metadata.comment || "--") +
        "<br>Track: " + (parseInt(subtune) + 1) + " of " + subtune_count + tracks
    );
}

function buildTracksHTML(filename, tracks) {
    ret = "<br>";
    filename = filename.replace(/'/g, '\\\'').replace(/"/g, '\\"');
    for (i = 0; i < tracks; i++) {
        ret += "<a href='#' onclick=\"return play('" + (filename) + "'," + i + ")\">" + (parseInt(i) + 1) + "</a> ";
    }
    return ret;
}

function updateSongInfoDropped(filename, subtune) {
    var subtune_count = Module.ccall("gme_track_count", "number", ["number"], [emu]);

    if (Module.ccall("gme_track_info", "number", ["number", "number", "number"], [emu, ref, subtune]) != 0)
        console.error("could not load metadata");

    var metadata = parse_metadata(Module.getValue(ref, "*"));

    var element = document.getElementById("metadata");
    tracks = buildTracksHTMLDropped(filename, subtune_count);
    $("#song_info").html("FileName: " + getFileName(filename) + "<br>" + metadata.author + " - " + metadata.game + ", Track " + (parseInt(subtune) + 1) + " of " + subtune_count + tracks);
}

function buildTracksHTMLDropped(filename, tracks) {
    ret = "<br>";
    filename = filename.replace(/'/g, '\\\'').replace(/"/g, '\\"');
    for (i = 0; i < tracks; i++) {
        ret += "<a href='#' onclick=\"return playDropped('" + filename + "'," + i + ")\">" + (parseInt(i) + 1) + "</a>&nbsp;";
    }
    return ret;
}

var ref;
var emu;
var node;

function play_song(filename, subtune) {

    if (node) {
        node.disconnect();
        node = null;
    }
    var xhr = new XMLHttpRequest();
    xhr.open("GET", filename, true);
    xhr.responseType = "arraybuffer";
    xhr.onerror = function (e) {
        message(e);
    };
    xhr.onload = function (e) {
        if (this.status == 404) {
            message("not found");
            return;
        }
        var payload = new Uint8Array(this.response);
        playMusicData(payload, subtune);
        updateSongInfo(filename, subtune);
    };
    xhr.send();
}

function playDropped(filename, subtune) {

    updateSongInfoDropped(filename, subtune);

    if (Module.ccall("gme_start_track", "number", ["number", "number"], [emu, subtune]) != 0)
        message("could not load track");
    setChannels();


    return false;
}

$(() => {
    $("#files a").click(e => {
       play('songs/' + $(e.target).text());
    });
    $("html").bind("drop", function(e){
        e.stopPropagation();
        e.preventDefault();

        handleDroppedFiles(e.originalEvent.dataTransfer.files);
        message("file dropped");
    }).bind("dragenter dragover", false);
});

function handleDroppedFiles(files){
    var file = files[0];
    message("dropped file : "+file.name + " ("+file.type+")");

    if(getFileExtension(file.name).toLowerCase() === "gbs" ||
        getFileExtension(file.name).toLowerCase() === "nsf" ||
        getFileExtension(file.name).toLowerCase() === "spc" ||
        getFileExtension(file.name).toLowerCase() === "gym"
    ){
        // create the reader to access the local file (note: browser have different security restrictions)
        var reader = new FileReader();
        reader.onerror = function(){message("reader error");}; // event handle on failure
        // load the file as array buffer
        reader.readAsArrayBuffer(file);

        // init the reader event handlers
        reader.onload = function (e)
        {
            message("file loaded.");
            var arrayBuffer = new Uint8Array(e.target.result);
            playMusicData(arrayBuffer, 0);
            updateSongInfoDropped(file.name, 0);

        }; // event handle on success
    }else{
        message("not supported file type.");
    }
}

function playMusicData(payload, subtune) {
    message("subtune:" + subtune);

    ref = Module.allocate(1, "i32", Module.ALLOC_NORMAL);

    var samplerate = audioContext.sampleRate;

    if (Module.ccall("gme_open_data", "number", ["array", "number", "number", "number"], [payload, payload.length, ref, samplerate]) != 0) {
        console.error("gme_open_data failed.");
        return;
    }
    emu = Module.getValue(ref, "i32");

    var subtune_count = Module.ccall("gme_track_count", "number", ["number"], [emu]);

    Module.ccall("gme_ignore_silence", "number", ["number"], [emu, 1]);

    var voice_count = Module.ccall("gme_voice_count", "number", ["number"], [emu]);
    message("Channel count: ", voice_count);
    message("Track count: ", subtune_count);

    if (voice_count >= 1) {
        $("#ch1").removeAttr("disabled")
    } else {
        $("#ch1").attr("disabled", true)
    }
    if (voice_count >= 2) {
        $("#ch2").removeAttr("disabled")
    } else {
        $("#ch2").attr("disabled", true)
    }
    if (voice_count >= 3) {
        $("#ch3").removeAttr("disabled")
    } else {
        $("#ch3").attr("disabled", true)
    }
    if (voice_count >= 4) {
        $("#ch4").removeAttr("disabled")
    } else {
        $("#ch4").attr("disabled", true)
    }
    if (voice_count >= 5) {
        $("#ch5").removeAttr("disabled")
    } else {
        $("#ch5").attr("disabled", true)
    }
    if (voice_count >= 6) {
        $("#ch6").removeAttr("disabled")
    } else {
        $("#ch6").attr("disabled", true)
    }
    if (voice_count >= 7) {
        $("#ch7").removeAttr("disabled")
    } else {
        $("#ch7").attr("disabled", true)
    }
    if (voice_count >= 8) {
        $("#ch8").removeAttr("disabled")
    } else {
        $("#ch8").attr("disabled", true)
    }

    $("#tempo").val(1.0);
    $("#tempolabel").html(1.0);

    if (Module.ccall("gme_start_track", "number", ["number", "number"], [emu, subtune]) != 0)
        message("could not load track");
    setChannels();

    var bufferSize = 4096;
    var inputs = 2;
    var outputs = 2;

    if (!node && audioContext.createJavaScriptNode) node = audioContext.createJavaScriptNode(bufferSize, inputs, outputs);
    if (!node && audioContext.createScriptProcessor) node = audioContext.createScriptProcessor(bufferSize, inputs, outputs);

    var buffer = Module.allocate(bufferSize * 16, "i16", Module.ALLOC_NORMAL);

    var INT16_MAX = Math.pow(2, 16) - 1;

    node.onaudioprocess = function (e) {
        if (Module.ccall("gme_track_ended", "number", ["number"], [emu]) == 1) {
            node.disconnect();
            message("end of stream");
            return;
        }

        var channels = [e.outputBuffer.getChannelData(0), e.outputBuffer.getChannelData(1)];

        var err = Module.ccall("gme_play", "number", ["number", "number", "number"], [emu, bufferSize * 2, buffer]);
        for (var i = 0; i < bufferSize; i++)
            for (var channel = 0; channel < e.outputBuffer.numberOfChannels; channel++)
                channels[channel][i] = Module.getValue(buffer +
                    // Interleaved channel format
                    // frame number * bytes per frame +
                    i * 4 +
                    // channel number * bytes per sample
                    channel * 2,
                    // 16-bit integer
                    "i16") / INT16_MAX;
    };
    node.connect(filterNode);

    window.savedReferences = [audioContext, node];
}

