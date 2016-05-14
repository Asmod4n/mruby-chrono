assert('Steady clock advances steadily') do
  assert_true(Chrono::Steady.now < Chrono::Steady.now, 'expected the clock to advance steadily')
end
