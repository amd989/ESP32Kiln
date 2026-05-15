const valuesUrl = "/PIDKiln_vars.json";

var program_status=0;	// status of the ESP32Kiln program 0-none,1-ready... etc = check ESP32Kiln.h

function dis_all_bttn(){
	// Dashboard buttons (handle by name attribute)
	$("button[name='prog_start']").attr("disabled", true);
	$("button[name='prog_end']").attr("disabled", true);
	$("button[name='prog_pause']").attr("disabled", true);
	$("button[name='prog_abort']").attr("disabled", true);
}

function ena_bttn_name(name){
  $("button[name='" + name + "']").attr("disabled", false);
}

function change_program_status(ns){
  console.log("Program status changed on "+ns);
  if(ns==0){	// program not loaded - disable all buttons
	dis_all_bttn();
  }else if(ns==2 || ns==6 || ns==7){	// program running/waiting 4 threshold/calibrating - enable abort (and pause/stop when not calibrating)
	dis_all_bttn();
	if(ns!=7) ena_bttn_name("prog_pause");
	if(ns!=7) ena_bttn_name("prog_end");
	ena_bttn_name("prog_abort");
	if(!chart_update_id) chart_update_id=setTimeout(chart_update, 30000);
  }else if(ns==3){	// program paused - enable start, abort, stop
	dis_all_bttn();
	ena_bttn_name("prog_start");
	$("button[name='prog_start']").html('<i class="bi bi-play-fill me-2"></i>Resume Program');
	ena_bttn_name("prog_end");
	ena_bttn_name("prog_abort");
	if(!chart_update_id) chart_update_id=setTimeout(chart_update, 30000);
  }else{		// program ready, stopped, aborted, failed - but loaded, enable start
	dis_all_bttn();
	ena_bttn_name("prog_start");
	$("button[name='prog_start']").html('<i class="bi bi-play-fill me-2"></i>Start Program');
	clearTimeout(chart_update_id);
  }
  program_status=ns;
}

function executeQuery() {
 $.ajax({
   url : valuesUrl,
   dataType : 'json'
 })
 .done((res) => {
   if(res.program_status!=program_status) change_program_status(res.program_status);

   res.pidkiln.forEach(el => {
     const element = $(el.html_id);
     // Check if element is an input/textarea or a div/span
     if (element.is('input, textarea, select')) {
       element.val(el.value);
     } else {
       // For div elements in status bar, update text content with units
       let displayValue = el.value;
       if (el.html_id === '#kiln_temp' || el.html_id === '#set_temp') {
         displayValue = el.value + '°C';
       } else if (el.html_id === '#heat_time') {
         displayValue = el.value + '%';
       }
       element.text(displayValue);
     }
   });

 })

 setTimeout(executeQuery, 5000);
}


$(document).ready(function() {
  executeQuery();
});

change_program_status(1);   // assume program is ready on start