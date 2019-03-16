(function($){
  $(function(){
    $('.sidenav').sidenav();
  });
})(jQuery);


ALL_MESSAGES = {}


function fetchData() {

  $.getJSON('/data.json', function(data) {
    for (var j=0; j < data.length; j++) {
      var informations = data[j];
      console.log(informations);
    }
  });
}


function createGraph(name, value, timestamp) {
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

function updateGraph(name, value, timestamp) {
  ALL_MESSAGES[name][0].x.push(timestamp);
  ALL_MESSAGES[name][0].y.push(value);
  Plotly.redraw(name);
}
