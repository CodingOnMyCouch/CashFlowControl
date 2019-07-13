#include <cashflowctrl.hpp>

cashflowctrl::cashflowctrl(eosio::name receiver, eosio::name code, datastream<const char*> ds):contract(receiver, code, ds),
                                                                               state_pattern(this->_self, this->_self.value),
                                                                               state(state_pattern.exists() ? state_pattern.get() : default_parameters()){}

 
// destructor
cashflowctrl::~cashflowctrl()
{
  this->state_pattern.set(this->state, this->_self);
  print(this->state.toString());
}

void cashflowctrl::login(eosio::name username)
{
  // require the authority of the username
  require_auth(username);
  
  //balances _balances(_self, username.value);
  accounts _acnts(_self, username.value);
  
  
  auto it = _acnts.find(username.value);
  
  if(it == _acnts.end()){
    it = _acnts.emplace(username, [&](auto& new_acc){
      new_acc.contract = username;
      new_acc.balance = eosio::asset(0, eosio::symbol("TOKEN", 4));
    });
  } else {
    print(name{username}, " is already a registered member");
  }

  cashflowctrl::action_receipt(username, "successfully logged and now registered into the system");
}



void cashflowctrl::getbalance(eosio::name username)
{
  require_auth(username);
  
  accounts _balances(_self, username.value);
  
  auto it = _balances.find(username.value);
  if(it != _balances.end())
  {
    it->balance.print();
  }

  cashflowctrl::action_receipt(username, "successfully read the balance of the user");
}


void cashflowctrl::deposit(const eosio::name from, const eosio::name to, const eosio::asset quantity, std::string& memo)
{
  auto self = get_self(); // get_self() - get this contract name
  
  // avoid the invalid iterator bug when this deposit function is called by the withdraw action later on
  if(from == self){
    
    add_balance(get_first_receiver(), to, quantity, self);
    
    transfer_to_acnt( from, quantity );
    
  } else if(to == self){
    // first check that the person exists in the data table
    accounts _find_entry(_self, from.value);
    auto itr = _find_entry.find(from.value);
    auto& p_itr = *itr;
    eosio::check(p_itr.contract == from, "that name doesn't exist in the table, plase login or register");
   
    add_balance(get_first_receiver(), to, quantity, self);
    
    transfer_to_acnt( from, quantity );
    
    print("Deposit complete to : ", name{to});
  }
  cashflowctrl::action_receipt(from, "successfully made a deposit onto the ctr2 contract");
 }


void cashflowctrl::setdaylimit(eosio::name owner, eosio::asset quantity)
{
  require_auth(owner); // requires the authority of the owner to execute this tx
  
  // update state 
  this->state.payee = owner;
  this->state.limit = quantity;
  
  // if the quantity amount is greater than 0
  if(quantity.amount > 0)
  {
    // start the timer
    this->state.start = time_point_sec(current_time_point());
    this->state.finish = time_point_sec(current_time_point() + hours(24));
  }
  
  this->state.toString();
}


void cashflowctrl::withdraw(eosio::name contract, eosio::name from, name to, asset quantity, std::string& memo)
{
    auto self = get_self();
    require_auth(self);
    
    time_point_sec zero_time = eosio::time_point_sec(0);
    
    eosio::check(quantity.symbol == symbol("TOKEN", 4), "can only withdraw TOKEN's ");
    eosio::check(quantity.amount > 0, "withdraw amount must be positive");
    
    if(current_time_point().sec_since_epoch() <= this->state.finish.utc_seconds && this->state.limit.amount > 0)
    {
      check(this->state.limit.amount >= quantity.amount, "cannot withdraw more than the limit");
      
      sub_balance(contract, from, quantity);
  
      // updates the token balance in the recipient  account
      transfer_from_acnt(to, quantity);
      
      // updates the token balance in the sendee account
      transfer_to_acnt( from, quantity );
      
      // Inline transfer
      cashflowctrl::transfer_action t_action(contract, {_self, "active"_n});
      
      t_action.send(_self, to, quantity, memo);
    
    }else if(this->state.limit.amount == 0 || this->state.start == zero_time || this->state.finish == zero_time){
      // reset the limit, start and finish state varables
      state = default_parameters();
      
      sub_balance(contract, from, quantity);
  
      // updates the token balance in the recipient  account
      transfer_from_acnt(to, quantity);
      
      // updates the token balance in the sendee account
      transfer_to_acnt( from, quantity );
      
      // Inline transfer
      cashflowctrl::transfer_action t_action(contract, {_self, "active"_n});
      
      t_action.send(_self, to, quantity, memo);
    }
    cashflowctrl::action_receipt(from, "successfully made a withdrawal from the ctr contract");
}




void cashflowctrl::add_balance(eosio::name contract, eosio::name owner, eosio::asset value, eosio::name ram_payer)
{
  accounts to_acnts(get_self(), owner.value);

  if(contract == get_self()){
    //auto it = to_acnts.find(contract.value);
    auto it = to_acnts.find(owner.value);
    
    if(it != to_acnts.end()){
      to_acnts.modify(it, ram_payer, [&](auto& new_acc){
        new_acc.balance += value;
      });
    }  
  } 
}




void cashflowctrl::sub_balance(eosio::name contract, eosio::name owner, eosio::asset value)
{
  accounts from_acnts(get_self(), owner.value);

  //auto itr = from_acnts.find(contract.value);
  auto itr = from_acnts.find(owner.value);
  
  if(itr != from_acnts.end()){
    from_acnts.modify(itr, same_payer, [&](auto& new_acc){
      new_acc.balance -= value;
    });
  }
}



void cashflowctrl::transfer_to_acnt( const name& owner, const asset& amount )
{
    accounts to_acc(_self, owner.value);
    
    auto itr = to_acc.find( owner.value );
  
    if(itr != to_acc.end()){
      to_acc.modify(itr, _self, [&](auto& new_acc){
        new_acc.balance.amount += amount.amount;
      });
    }
    
}
 
 
void cashflowctrl::transfer_from_acnt( const name& owner, const asset& amount )
{
  accounts from_acc(_self, owner.value);
  
  auto itr = from_acc.require_find( owner.value, "must deposit to ctr smart contract first" );
  
  from_acc.modify( itr, same_payer, [&]( auto& new_acc ) {
      new_acc.balance.amount -= amount.amount;
  });
}
