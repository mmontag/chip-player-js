var audioContext;
var analyserNode;
var filterNode;
var timeDomainData;
var frequencyData;
var canvasTimeDomain;
var canvasFrequency;
var canvasSpectrogram;
var ctxTD;
var ctxFq;
var ctxSp;
var tempCanvas;
var tempCtx;
var lastCalledTime;
var fps;
var hot = new chroma.ColorScale({
    colors: ['#000000', '#0000a0', '#6000a0', '#662781',
        '#dd1440', '#f0b000', '#ffffa0', '#ffffff'],
    mode: 'rgb',
    limits: [0, 255]
});

if (!window.requestAnimationFrame) {
    if (window.webkitRequestAnimationFrame) {
        window.requestAnimationFrame = window.webkitRequestAnimationFrame;
    } else if (window.mozRequestAnimationFrame) {
        window.requestAnimationFrame = window.mozRequestAnimationFrame;
    } else {
        alert("your brwoser has no requestAnimationFrame support.");
    }
}

function message(msg) {
    now = new Date();
    $("#message").html(now.toLocaleString() + "." + formatInt(now.getMilliseconds(), 3) + " " + msg + "<br />" + $("#message").html());
}

function formatInt(value, digit) {
    padding = "";
    for (i = 0; i < digit; i++) {
        if (Math.floor(value / Math.pow(10, i)) == 0) {
            padding = padding + "0";
        }
    }
    return padding + value;
}

$(function () {
    init();
});

function init() {
    message("initializing...");
    try {
        if (window.webkitAudioContext) {
            audioContext = new webkitAudioContext();
        }
        if (window.AudioContext) {
            audioContext = new AudioContext();
        }
        analyserNode = audioContext.createAnalyser();
        analyserNode.minDecibels = -90;
        analyserNode.maxDecibels = -30;
        timeDomainData = new Uint8Array(analyserNode.frequencyBinCount);
        frequencyData = new Uint8Array(analyserNode.frequencyBinCount);

        message("analyserNode.fftSize: " + analyserNode.fftSize);
        message("analyserNode.frequencyBinCount: " + analyserNode.frequencyBinCount);
        message("initial analyserNode.smoothingTimeConstant: " + analyserNode.smoothingTimeConstant);

        message("initial analyserNode.minDecibels / maxDecibels: " + analyserNode.minDecibels + " / " + analyserNode.maxDecibels);

        message("set analyserNode.smoothingTimeConstant to 0.");
        analyserNode.smoothingTimeConstant = 0.0;

        filterNode = audioContext.createBiquadFilter();
        filterNode.connect(audioContext.destination);
        filterNode.connect(analyserNode);

        filterNode.type = getBiquadFilterType(7);//ALL_PASS
        filterNode.frequency.value = 5000;
        filterNode.Q.value = 5;

        message("canvas setup");
        //canvas setup

        canvasTimeDomain = $("#canvasTimeDomain")[0];
        ctxTD = canvasTimeDomain.getContext('2d');

        canvasFrequency = $("#canvasFrequency")[0];
        ctxFq = canvasFrequency.getContext('2d');

        canvasSpectrogram = $("#canvasSpectrogram")[0];
        ctxSp = canvasSpectrogram.getContext("2d");

        // create a temp canvas we use for copying
        tempCanvas = document.createElement("canvas");
        tempCtx = tempCanvas.getContext("2d");
        tempCanvas.width = canvasSpectrogram.width;
        tempCanvas.height = canvasSpectrogram.height;

        message("[initialized.]");

    } catch (e) {
        message("[" + e + "]");
        message("Web Audio API is not supported in this browser.");
        message("please try with Google Chrome.");
    }
    updateFrame();

}

function updateFrame() {
    analyserNode.getByteTimeDomainData(timeDomainData);
    drawTimeDomain(timeDomainData);
    analyserNode.getByteFrequencyData(frequencyData);
    drawFrequency(frequencyData);
    drawSpectrogram(frequencyData);

    if (!lastCalledTime) {
        lastCalledTime = new Date().getTime();
        fps = 0;
    } else {
        delta = (new Date().getTime() - lastCalledTime) / 1000;
        lastCalledTime = new Date().getTime();
        fps = 1 / delta;
    }
    $("#fps").html(Math.round(fps) + "FPS");


    requestAnimationFrame(updateFrame);
}

function convertoS2MS(sec) {
    var m = Math.floor((sec / 60) % 60);
    var s = Math.floor(sec % 60);
    if (m < 10) {
        m = "0" + m
    }
    if (s < 10) {
        s = "0" + s
    }
    return m + ":" + s;
}

function drawTimeDomain(data) {

    // ctxTD.beginPath();
    const height = canvasTimeDomain.height;
    const width = canvasTimeDomain.width;
    const stride = 1;
    ctxTD.fillStyle = "black";
    ctxTD.rect(0, 0, width, height);
    ctxTD.fill();
    ctxTD.beginPath();
    var l = Math.min(data.length, width);
    let j = 0;
    ctxTD.moveTo(-1, height / 2);
    for (var i = 0; i < l; i += stride, j++) {
        ctxTD.lineTo(j, data[i] - 128 + height / 2);
    }
    ctxTD.moveTo(width + 1, height / 2);
    ctxTD.closePath();
    ctxTD.strokeStyle = "white";
    ctxTD.stroke();
}

function drawFrequency(data) {
    ctxFq.beginPath();
    ctxFq.fillStyle = "black";
    ctxFq.rect(0, 0, canvasFrequency.width, canvasFrequency.height);
    ctxFq.fill();
    var value;
    ctxFq.beginPath();
    ctxFq.moveTo(0, -999);

    var gradient = ctxFq.createLinearGradient(0, 0, 0, 300);
    gradient.addColorStop(0, '#ffffff');
    gradient.addColorStop(0.1, '#ffffff');
    gradient.addColorStop(0.9, '#000000');
    gradient.addColorStop(1, '#000000');

    //ctxFq.fillStyle = gradient;
    //ctxFq.fillStyle = "gray";
    var fqHeight = canvasFrequency.height;
    var divider = (256.0 / fqHeight);
    for (var i = 0; i < data.length; ++i) {
        value = fqHeight - data[i] / divider;// - 128 + canvasFrequency.height / 2;
        ctxFq.lineTo(i, value);
        ctxFq.fillStyle = hot.getColor(data[i]).hex();
        ctxFq.fillRect(i, fqHeight - data[i] / divider, 1, data[i] / divider);
    }
    ctxFq.moveTo(0, 999);
    ctxFq.closePath();
    // ctxFq.strokeStyle = "gray";
    // ctxFq.stroke();

}

function drawSpectrogram(array) {
    // copy the current canvas onto the temp canvas
    tempCtx.drawImage(canvasSpectrogram, 0, 0, canvasSpectrogram.width, canvasSpectrogram.height);
    var frameHeight = 3;
    // iterate over the elements from the array
    for (var i = 0; i < array.length; i++) {
        // draw each pixel with the specific color
        var value = array[i];
        ctxSp.fillStyle = hot.getColor(value).hex();

        // draw the line at the right side of the canvas
        //audioContext . fillRect(x, y, w, h)
        ctxSp.fillRect(i, 0, 1, frameHeight);
    }

    // set translate on the canvas
    ctxSp.translate(0, frameHeight);
    // draw the copied image
    ctxSp.drawImage(tempCanvas, 0, 0, canvasSpectrogram.width, canvasSpectrogram.height, 0, 0, canvasSpectrogram.width, canvasSpectrogram.height);

    // reset the transformation matrix
    ctxSp.setTransform(1, 0, 0, 1, 0, 0);
}

function getBiquadFilterType(index) {
    switch (index) {
        case 0:
            return "lowpass";
        case 1:
            return "highpass";
        case 2:
            return "bandpass";
        case 3:
            return "lowshelf";
        case 4:
            return "highshelf";
        case 5:
            return "peaking";
        case 6:
            return "notch";
        case 7:
            return "allpass";
    }
}

function filterSetup() {
    var selectedIndex = $('input[name="filterType"]:checked').val();
//	filterNode.type = parseInt(selectedIndex);
    filterNode.type = getBiquadFilterType(parseInt(selectedIndex));
    $("#freqlabel").html(parseFloat($("#freq").val()) + "Hz");
    $("#qlabel").html(parseFloat($("#q").val()));
    $("#gainlabel").html(parseFloat($("#gain").val()));

    filterNode.frequency.value = parseFloat($("#freq").val());
    filterNode.Q.value = parseFloat($("#q").val());
    filterNode.gain.value = parseFloat($("#gain").val());
    //console.log("selectedIndex"+selectedIndex);
    switch (selectedIndex) {
        case "0"://LPF
            $("#q").removeAttr("disabled");
            $("#gain").attr("disabled", true);
            break;
        case "1"://HPF
            $("#q").removeAttr("disabled");
            $("#gain").attr("disabled", true);
            break;
        case "2"://BPF
            $("#q").removeAttr("disabled");
            $("#gain").attr("disabled", true);
            break;
        case "3"://LowShelf
            $("#q").attr("disabled", true);
            $("#gain").removeAttr("disabled");
            break;
        case "4"://HighShelf
            $("#q").attr("disabled", true);
            $("#gain").removeAttr("disabled");
            break;
        case "5"://Peaking
            $("#q").removeAttr("disabled");
            $("#gain").removeAttr("disabled");
            break;
        case "6"://Notch
            $("#q").removeAttr("disabled");
            $("#gain").attr("disabled", true);
            break;
        case "7"://AllPass
            $("#q").removeAttr("disabled");
            $("#gain").attr("disabled", true);
            break;
        default:
            break;
    }
}

function updateTempo() {
    var val = parseFloat($('#tempo').val()) || 1.0;
    $('#tempolabel').html(val.toFixed(1));
    Module.ccall("gme_set_tempo", "number", ["number"], [emu, val]);
}
