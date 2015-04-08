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
