var chart = new Highcharts.chart('container', {
  chart : {type : 'spline'},
  title : {text : ''},
  data : {csvURL : './data.csv', enablePolling : true, dataRefreshRate : 10},
  yAxis : [
    {
      title : {text : 'Temperature'},
      labels : {format : '{value} °C'},
      plotLines : [ {color : 'rgba(0, 165, 0, 1)', value : 21, width : 2} ],
      plotBands : [ {from : 20.5, to : 21.5, color : 'rgba(0, 165, 0, 0.1)'} ],
      minRange : 2
    },
    {title : {text : undefined}, labels : {enabled : false}, min : 0, max : 10}
  ],
  series : [ {yAxis : 0}, {yAxis : 1} ],
  credits : {enabled : false}
});

function updateBar(x) {
  chart.yAxis[0].options.plotLines[0].value = x;
  chart.yAxis[0].options.plotBands[0].from = x - 0.5;
  chart.yAxis[0].options.plotBands[0].to = x - -0.5;
  chart.yAxis[0].update();
}

function updateCSS(state) {
  if (state == 'on') {
    element = document.querySelector('#start');
    element.style.backgroundColor = '#9cd79e';
    element = document.querySelector('#stop');
    element.style.backgroundColor = '#f44336';
  } else if (state == 'off') {
    element = document.querySelector('#start');
    element.style.backgroundColor = '#4CAF50';
    element = document.querySelector('#stop');
    element.style.backgroundColor = '#f88981';
  }
}

function updateSTemp() {
  var temp = document.getElementById("sTemp").value;
  fetch('/setpoint?value=' + temp, {
    method : 'POST',
    headers : {'Accept' : 'text/plain', 'Content-Type' : 'text/plain'}
  })
      .then((response) => {
        if (!response.ok) {
          console.log("Couldn't update setpoint.");
          throw new Error(`HTTP error! Status: ${response.status}`);
        }
        updateBar(temp);
      })
      .catch((error) => { console.error('Error:', error); });
}

function setState(state) {
  fetch('/systemState?value=' + state, {
    method : 'POST',
    headers : {'Accept' : 'text/plain', 'Content-Type' : 'text/plain'}
  })
      .then((response) => {
        if (!response.ok) {
          console.log("Couldn't update system state.");
          throw new Error(`HTTP error! Status: ${response.status}`);
        }
        updateCSS(state);
      })
      .catch((error) => { console.error('Error:', error); });
}

function updateState() {
  // don't update the setpoint if it's currently edited
  if (!(document.activeElement === document.getElementById("sTemp"))) {
    fetch('/setpoint', {method : 'GET', headers : {'Accept' : 'text/plain'}})
        .then((response) => {
          if (!response.ok) {
            console.log("Couldn't get setpoint.");
            throw new Error(`HTTP error! Status: ${response.status}`);
          }
          return response.text()
        })
        .then((temp) => {
          document.getElementById("sTemp").value = temp;
          updateBar(temp);
        })
        .catch((error) => { console.error('Error:', error); });
  }

  fetch('/systemState', {method : 'GET', headers : {'Accept' : 'text/plain'}})
      .then((response) => {
        if (!response.ok) {
          console.log("Couldn't get system state.");
          throw new Error(`HTTP error! Status: ${response.status}`);
        }
        return response.text()
      })
      .then((state) => { updateCSS(state); })
      .catch((error) => { console.error('Error:', error); });
}

setInterval(updateState, 1000);
