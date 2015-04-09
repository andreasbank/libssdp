function group_action_visibility() {
  var theform = document.getElementById('group_action_form');
  var thefield = document.getElementById('group_action_field');

  for(var i = 0; i < theform.elements.length; i++ ) {
    if(theform.elements[i].type == 'checkbox') {
      if(theform.elements[i].checked == true) {
        thefield.style.display = 'table-row';
        return;
      }
    }
  }
  thefield.style.display = 'none';

}

function dropdown_check() {
  var action = document.getElementById('select_action');
  switch(action.value) {
  case 'dispatch_devices':
    return set_portal_ips();
    break;
  case 'factory_default':
    return confirm('Are you sure you want to factory default the selected device(s)?');
    break;
  default:
    break;
  }
}

function set_portal_ips() {
  var hidden_input = document.getElementById('portal_ips');
  hidden_input.value = prompt('Specify AVHS Portal IP\'s:', '<comma separated list of IP\'s>');
  if(hidden_input.value.length > 0) {
    return true;
  }
  return false;
}
