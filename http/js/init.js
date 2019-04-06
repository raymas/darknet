(function($){
  $(function(){
    $('.sidenav').sidenav();
  });
})(jQuery);


ALL_MESSAGES = {}

// Naive csv reader


function readSingleFile(e) {
  var file = e.target.files[0];
  if (!file) {
    return;
  }
  var reader = new FileReader();
  reader.onload = function(e) {
    var contents = e.target.result;
    handleContents(contents, ";");
  };
  reader.readAsText(file);
}

function handleContents(contents, delimiter) {
  var allTextLines = contents.split(/\r\n|\n/);

  var headers = allTextLines[0].split(delimiter);
  for (var i=0; i < headers.length; i++) {
    ALL_MESSAGES[headers[i]] = []
  }

  for (var j=1; j < allTextLines.length; j++) {
    var row = allTextLines[j].split(delimiter);
    for (var i=0; i < row.length; i++) {
      if (row[i] != '') {
        ALL_MESSAGES[headers[i]].push(parseFloat(row[i]));
      }
    }
  }

  var x = ALL_MESSAGES["batch"];
  for (var key in ALL_MESSAGES) {
    if (ALL_MESSAGES.hasOwnProperty(key)) {
      if (key != "batch" && key != "") {
        createGraphLog(key, x, ALL_MESSAGES[key], (key == "map")? 'markers' : 'lines');
      }
    }
  }

}

document.getElementById('log-input').addEventListener('change', readSingleFile, false);

function fetchData() {
  $.getJSON('/data.json', function(data) {
    for (var j=0; j < data.length; j++) {
      var informations = data[j];
      console.log(informations);
    }
  });
}


function createGraphLog(name, x, y, mode) {
  var layout = {
    title: name,
    xaxis: {
      title: 'Iterations',
      showgrid: true,
      zeroline: true
    },
    yaxis: {
      title: '',
      showline: false
    },
  };

  var data = [{
    x: x,
    y: y,
    type: 'scatter',
    mode: mode
  }];

  console.log(data);

  var mainContainer = document.getElementById('mainContent');

  var newplotdiv = document.createElement('div');
  newplotdiv.id = name;
  newplotdiv.classList.add("control-panel");

  mainContainer.appendChild(newplotdiv);

  Plotly.newPlot(name, data, layout, {responsive: true});
}

// Live update

function createGraphLive(name, value, timestamp) {
  var layout = {
    title: name,
    xaxis: {
      title: 'Iterations',
      showgrid: true,
      zeroline: true
    },
    yaxis: {
      title: '',
      showline: false
    },
  };

  var data = [{
    x: [timestamp],
    y: [value],
    type: 'scatter'
  }];

  console.log(data);

  ALL_MESSAGES[name] = data;

  var mainContainer = document.getElementById('mainContent');

  var newplotdiv = document.createElement('div');
  newplotdiv.id = name;
  newplotdiv.classList.add("control-panel");

  mainContainer.appendChild(newplotdiv);

  Plotly.newPlot(name, ALL_MESSAGES[name], layout, {responsive: true});
}

function updateGraphLive(name, value, timestamp) {
  ALL_MESSAGES[name][0].x.push(timestamp);
  ALL_MESSAGES[name][0].y.push(value);
  Plotly.redraw(name);
}
