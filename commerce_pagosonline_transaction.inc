<?php
function commerce_pagosonline_transaction_view($id_transaction) {
  $result = db_select('pagosonline_log', 'p')
    ->fields('p')
    ->condition('id', $id_transaction)
    ->execute()
    ->fetchAssoc();
  if ($result) {
    
    $content = array();
    module_load_include('inc', 'commerce_pagosonline');
    $data = array();
    
    foreach ($result as $key => $value) {
      $data[] = array($key, $value);
    }
    $content['transation'] = array(
      '#prefix' => '<div>',
      'suffix' => '</div>',
    );
    $content['transation']['title'] = array(
      '#markup' => '<h2>' . t('TRANSATION') . '</h2>', 
    );
    
    $content['transation']['detail'] = array(
      '#markup' => theme('table', array('header' => array('Name', 'Value'), 'rows' => $data)),
    );
    
    $content['order'] = array(
      '#prefix' => '<div>',
      'suffix' => '</div>',
      '#markup' => l(t('View Payments'), 'admin/commerce/orders/' . $result['order_id']. '/payment'),
    );
    
    return $content;
    
  }
  else {
    drupal_set_message(t('Record no found'), 'error');
  }
}

function commerce_pagosonline_view_transaction_validate($id_transaction, $option) {
  return ((user_access('pagosonline detail transaction') && $option == md5($id_transaction . '~' . 'VIEW-TRANSATION')));
}

